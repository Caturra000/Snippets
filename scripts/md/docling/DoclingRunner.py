import argparse
import gc
import importlib.metadata
import json
import logging
import os
import re
import sys
import time
import traceback
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def configure_streams():
    # Merge stderr into stdout before importing the model stack.
    sys.stdout.reconfigure(
        encoding="utf-8",
        errors="backslashreplace",
        line_buffering=True,
    )
    sys.stdout.flush()
    sys.stderr.flush()

    try:
        os.dup2(sys.stdout.fileno(), sys.stderr.fileno())
    except OSError:
        pass

    sys.stderr = sys.stdout


def positive_int(value):
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError("Must be greater than zero.")
    return result


def page_range(value):
    match = re.fullmatch(r"(\d+)(?:-(\d+))?", value.strip())
    if not match:
        raise argparse.ArgumentTypeError("Use a page range such as 1-5.")

    start = int(match.group(1))
    end = int(match.group(2) or start)

    if start < 1 or end < start:
        raise argparse.ArgumentTypeError(
            "Pages start at 1; the end must not precede the start."
        )
    return start, end


def make_parser():
    parser = argparse.ArgumentParser(
        description="Local Docling batch converter with per-document folders."
    )
    parser.add_argument("files", nargs="+")
    parser.add_argument(
        "--to", action="append",
        choices=["md", "json", "yaml", "html", "text"],
    )
    parser.add_argument("--device", choices=["cpu", "cuda"], default="cuda")
    parser.add_argument("--pipeline", choices=["standard"], default="standard")
    parser.add_argument(
        "--artifacts-path", type=Path, default=ROOT / "models"
    )
    parser.add_argument("--output", type=Path, default=ROOT / "output")
    parser.add_argument(
        "--image-export-mode",
        choices=["referenced", "embedded", "placeholder"],
        default="referenced",
    )

    parser.add_argument(
        "--ocr", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--ocr-engine", choices=["rapidocr"], default="rapidocr",
        help="This runner uses RapidOCR with the torch backend.",
    )
    parser.add_argument("--ocr-lang", default="ch")
    parser.add_argument(
        "--ocr-mode",
        choices=[
            "default",
            "full_page",
            "layout_regions",
            "pdf_aware_layout_regions",
        ],
        default="default",
    )
    parser.add_argument("--force-ocr", action="store_true")
    parser.add_argument(
        "--ocr-model-size", choices=["tiny", "small", "medium"],
        default="small",
    )

    parser.add_argument(
        "--tables", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--table-mode", choices=["fast", "accurate"], default="accurate"
    )
    parser.add_argument(
        "--cell-matching",
        action=argparse.BooleanOptionalAction,
        default=True,
    )

    parser.add_argument(
        "--enrich-formula", action=argparse.BooleanOptionalAction,
        default=False,
    )
    parser.add_argument(
        "--enrich-code", action=argparse.BooleanOptionalAction,
        default=False,
    )
    parser.add_argument(
        "--pdf-backend",
        choices=["docling_parse", "pypdfium2"],
        default="docling_parse",
    )
    parser.add_argument("--page-range", type=page_range)
    parser.add_argument("--num-threads", type=positive_int, default=8)
    parser.add_argument("--layout-batch-size", type=positive_int, default=8)
    parser.add_argument("--ocr-batch-size", type=positive_int, default=8)
    parser.add_argument("--table-batch-size", type=positive_int, default=8)
    parser.add_argument("--profiling", action="store_true")
    parser.add_argument("--save-profiling", action="store_true")
    parser.add_argument("-v", "--verbose", action="count", default=0)

    return parser


def new_output_folder(root, stem):
    # Do not overwrite earlier conversions or same-name documents.
    number = 1
    while True:
        name = stem if number == 1 else f"{stem}_{number}"
        folder = root / name
        try:
            folder.mkdir()
            return folder
        except FileExistsError:
            number += 1


def run():
    parser = make_parser()
    args = parser.parse_args()

    languages = [
        value.strip()
        for value in args.ocr_lang.split(",")
        if value.strip()
    ] or ["ch"]

    if len(languages) != 1:
        parser.error(
            "This RapidOCR runner accepts one recognition language/model "
            "at a time. Use --ocr-lang ch or --ocr-lang iso:zh-Hans."
        )

    if languages[0] == "ch_sim":
        parser.error(
            "ch_sim is an EasyOCR code. For RapidOCR use ch or iso:zh-Hans."
        )

    if args.force_ocr:
        if not args.ocr:
            parser.error("--force-ocr conflicts with --no-ocr.")
        args.ocr_mode = "full_page"
        print("NOTE: Prefer --ocr-mode full_page over --force-ocr.")

    logging.basicConfig(
        level=logging.DEBUG if args.verbose > 1 else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stdout,
        force=True,
    )

    import torch

    from docling.datamodel.accelerator_options import (
        AcceleratorDevice,
        AcceleratorOptions,
    )
    from docling.datamodel.backend_options import (
        ThreadedDoclingParseBackendOptions,
    )
    from docling.datamodel.base_models import ConversionStatus, InputFormat
    from docling.datamodel.pipeline_options import (
        OcrMode,
        PdfPipelineOptions,
        RapidOcrOptions,
        TableFormerMode,
        TableStructureOptions,
    )
    from docling.datamodel.settings import DEFAULT_PAGE_RANGE, settings
    from docling.document_converter import (
        DocumentConverter,
        ImageFormatOption,
        PdfFormatOption,
    )
    from docling_core.types.doc import ImageRefMode

    if args.device == "cuda":
        if not torch.cuda.is_available():
            raise RuntimeError("CUDA was requested but is not available.")
        print("GPU:", torch.cuda.get_device_name(0))

    print("Docling:", importlib.metadata.version("docling"))
    print("OCR: RapidOCR / torch /", args.ocr_model_size)
    print("OCR mode:", args.ocr_mode, "| enabled:", args.ocr)

    if not args.cell_matching:
        print(
            "NOTE: Cell matching is disabled. Compare table text carefully; "
            "this is not a general-purpose setting for scanned tables."
        )

    artifacts = args.artifacts_path.resolve()
    if not artifacts.is_dir():
        raise FileNotFoundError(f"Model directory not found: {artifacts}")

    settings.debug.profile_pipeline_timings = (
        args.profiling or args.save_profiling
    )

    formats = list(dict.fromkeys(args.to or ["md"]))
    image_mode = ImageRefMode(args.image_export_mode)
    needs_images = (
        args.image_export_mode != "placeholder"
        and any(fmt != "text" for fmt in formats)
    )

    options = PdfPipelineOptions(
        artifacts_path=artifacts,
        accelerator_options=AcceleratorOptions(
            device=AcceleratorDevice(args.device),
            num_threads=args.num_threads,
        ),
        do_ocr=args.ocr,
        ocr_options=RapidOcrOptions(
            backend="torch",
            lang=languages,
            mode=OcrMode(args.ocr_mode),
            model_size=args.ocr_model_size,
        ),
        do_table_structure=args.tables,
        table_structure_options=TableStructureOptions(
            mode=TableFormerMode(args.table_mode),
            do_cell_matching=args.cell_matching,
        ),
        do_formula_enrichment=args.enrich_formula,
        do_code_enrichment=args.enrich_code,
        generate_page_images=needs_images,
        generate_picture_images=needs_images,
        images_scale=2.0 if needs_images else 1.0,
        layout_batch_size=args.layout_batch_size,
        ocr_batch_size=args.ocr_batch_size,
        table_batch_size=args.table_batch_size,
    )

    if args.pdf_backend == "pypdfium2":
        from docling.backend.pypdfium2_backend import PyPdfiumDocumentBackend

        pdf_option = PdfFormatOption(
            pipeline_options=options,
            backend=PyPdfiumDocumentBackend,
        )
    else:
        pdf_option = PdfFormatOption(
            pipeline_options=options,
            backend_options=ThreadedDoclingParseBackendOptions(
                parser_threads=args.num_threads,
            ),
        )

    # One converter for the entire job: reuse initialized pipelines/models.
    converter = DocumentConverter(
        format_options={
            InputFormat.PDF: pdf_option,
            InputFormat.IMAGE: ImageFormatOption(pipeline_options=options),
        }
    )

    output_root = args.output.resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    ok = 0
    failed = 0
    batch_started = time.monotonic()

    for index, filename in enumerate(args.files, start=1):
        source = Path(filename).resolve()
        result = None
        document = None
        started = time.monotonic()

        print(f"\n[{index}/{len(args.files)}] {source}", flush=True)

        try:
            if not source.is_file():
                raise FileNotFoundError(source)

            folder = new_output_folder(output_root, source.stem)
            print("Output folder:", folder, flush=True)

            result = converter.convert(
                source,
                raises_on_error=False,
                page_range=args.page_range or DEFAULT_PAGE_RANGE,
            )

            if result.status != ConversionStatus.SUCCESS:
                details = "; ".join(
                    error.error_message for error in result.errors
                )
                raise RuntimeError(
                    f"Conversion status: {result.status}. {details}"
                )

            document = result.document
            images = Path("images")

            for fmt in formats:
                extension = "txt" if fmt == "text" else fmt
                target = folder / f"{source.stem}.{extension}"

                if fmt == "text":
                    target.write_text(
                        document.export_to_text(), encoding="utf-8"
                    )
                else:
                    method_name = {
                        "md": "save_as_markdown",
                        "json": "save_as_json",
                        "yaml": "save_as_yaml",
                        "html": "save_as_html",
                    }[fmt]

                    getattr(document, method_name)(
                        filename=target,
                        artifacts_dir=images,
                        image_mode=image_mode,
                    )

                if fmt == "md" and not target.read_text(
                    encoding="utf-8"
                ).strip():
                    raise RuntimeError("Markdown output is empty.")

            if args.profiling:
                print("Stage timings (overlapping stages are not additive):")
                for name, item in result.timings.items():
                    print(
                        f"  {name}: count={item.count}, "
                        f"total={float(item.total()):.3f}s"
                    )

            if args.save_profiling:
                timing_data = {
                    name: item.model_dump(mode="json")
                    for name, item in result.timings.items()
                }
                (folder / "timings.json").write_text(
                    json.dumps(timing_data, ensure_ascii=False, indent=2),
                    encoding="utf-8",
                )

            ok += 1
            print(
                f"[OK] {source.name} "
                f"{time.monotonic() - started:.2f}s -> {folder}",
                flush=True,
            )

        except Exception:
            failed += 1
            print(f"[FAIL] {source}", flush=True)
            traceback.print_exc()

        finally:
            document = None
            result = None
            gc.collect()

    print(
        f"\nDone. OK={ok} FAIL={failed} "
        f"elapsed={time.monotonic() - batch_started:.2f}s",
        flush=True,
    )
    return 1 if failed else 0


if __name__ == "__main__":
    configure_streams()

    try:
        exit_code = run()
    except KeyboardInterrupt:
        print("\nInterrupted.", flush=True)
        exit_code = 130
    except Exception:
        traceback.print_exc()
        exit_code = 2

    raise SystemExit(exit_code)