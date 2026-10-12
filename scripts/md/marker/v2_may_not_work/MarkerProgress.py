import gc
import importlib.metadata
import json
import os
import sys
import time
import traceback
from concurrent.futures import ThreadPoolExecutor, as_completed


EVENT_PREFIX = "@@MARKER_PROGRESS@@"


def redirect_stderr_to_stdout():
    """
    Put Python logging/tqdm and normal stdout into one pipe so PowerShell
    does not have to read two redirected streams concurrently.
    """
    try:
        sys.stdout.reconfigure(
            encoding="utf-8",
            errors="replace",
            line_buffering=True,
        )
    except Exception:
        pass

    try:
        os.dup2(sys.stdout.fileno(), sys.stderr.fileno())
    except Exception:
        sys.stderr = sys.stdout

    try:
        sys.stderr.reconfigure(
            encoding="utf-8",
            errors="replace",
            line_buffering=True,
        )
    except Exception:
        pass


def emit(event, **fields):
    payload = {
        "event": event,
        **fields,
    }
    print(
        EVENT_PREFIX + json.dumps(payload, ensure_ascii=False),
        flush=True,
    )


def install_surya_progress_patch():
    """
    Surya 0.22.1's OpenAI-compatible batch client waits for the complete
    executor.map() result and exposes no per-request progress.

    This replacement keeps the same request/retry behavior and output order,
    but emits one event whenever a final request completes.
    """
    from surya.inference.backends import openai_client
    from surya.inference.schema import BatchOutputItem

    def progress_chat_completions_batch(
        batch,
        client,
        model_name,
        max_tokens_default=2048,
        temperature=0.0,
        top_p=0.1,
        timeout=600.0,
        max_workers=None,
        max_retries=3,
        request_logprobs_default=True,
    ):
        if not batch:
            return []

        if max_workers is None:
            max_workers = min(64, len(batch))

        prompt_types = sorted(
            {
                str(getattr(item, "prompt_type", "unknown"))
                for item in batch
            }
        )
        phase = prompt_types[0] if len(prompt_types) == 1 else "mixed"

        emit(
            "batch_start",
            phase=phase,
            total=len(batch),
            prompt_types=prompt_types,
        )

        def process_one(item):
            result = openai_client._generate_one(
                item,
                client=client,
                model_name=model_name,
                max_tokens_default=max_tokens_default,
                temperature=temperature,
                top_p=top_p,
                timeout=timeout,
                request_logprobs_default=request_logprobs_default,
            )

            retries = 0

            while openai_client._should_retry(
                result,
                retries,
                max_retries,
            ):
                backoff = (
                    1.5 * (retries + 1)
                    if result.error
                    else 0
                )

                if backoff:
                    time.sleep(backoff)

                retry_temperature = min(
                    temperature + 0.2 * (retries + 1),
                    0.8,
                )
                retry_top_p = (
                    0.95
                    if not result.error
                    else top_p
                )

                result = openai_client._generate_one(
                    item,
                    client=client,
                    model_name=model_name,
                    max_tokens_default=max_tokens_default,
                    temperature=retry_temperature,
                    top_p=retry_top_p,
                    timeout=timeout,
                    request_logprobs_default=request_logprobs_default,
                )

                retries += 1

            return BatchOutputItem(
                raw=result.raw,
                token_count=result.token_count,
                error=result.error,
                mean_token_prob=result.mean_token_prob,
                logprobs=result.logprobs,
                metadata=item.metadata,
            )

        outputs = [None] * len(batch)
        completed = 0

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_map = {
                executor.submit(process_one, item): index
                for index, item in enumerate(batch)
            }

            for future in as_completed(future_map):
                index = future_map[future]
                item = batch[index]

                outputs[index] = future.result()
                completed += 1

                metadata = getattr(item, "metadata", None) or {}

                emit(
                    "batch_progress",
                    phase=phase,
                    done=completed,
                    total=len(batch),
                    page_idx=metadata.get("page_idx"),
                    block_idx=metadata.get("block_idx"),
                )

        emit(
            "batch_done",
            phase=phase,
            done=completed,
            total=len(batch),
        )

        # Preserve Surya's original input ordering.
        return outputs

    # Patch the defining module.
    openai_client.chat_completions_batch = (
        progress_chat_completions_batch
    )

    # LlamaCppBackend imported the function directly, so patch that module's
    # local reference too.
    from surya.inference.backends import llamacpp

    llamacpp.chat_completions_batch = (
        progress_chat_completions_batch
    )

    # Not required by the current Windows setup, but keep the wrapper usable
    # if a different backend is selected later.
    try:
        from surya.inference.backends import vllm

        vllm.chat_completions_batch = (
            progress_chat_completions_batch
        )
    except Exception:
        pass


def read_job(path):
    with open(path, "r", encoding="utf-8-sig") as file:
        job = json.load(file)

    files = [
        os.path.abspath(str(path))
        for path in job.get("files", [])
    ]
    marker_args = [
        str(value)
        for value in job.get("marker_args", [])
    ]
    output_dir = os.path.abspath(
        str(job.get("output_dir", "output"))
    )

    if not files:
        raise RuntimeError("The conversion job contains no input files.")

    return files, marker_args, output_dir


def parse_marker_options(
    first_file,
    marker_args,
    output_dir,
):
    """
    Let Marker/Click parse its own options, including Marker dynamically
    generated configuration options.
    """
    from marker.scripts.convert_single import convert_single_cli

    context = convert_single_cli.make_context(
        "marker_single",
        [first_file, *marker_args],
    )

    try:
        options = dict(context.params)
    finally:
        context.close()

    options.pop("fpath", None)

    # The drag-and-drop wrapper always owns the output directory.
    options["output_dir"] = output_dir

    return options


def run_job(job_path):
    files, marker_args, output_dir = read_job(job_path)

    try:
        marker_version = importlib.metadata.version("marker-pdf")
    except Exception:
        marker_version = "unknown"

    try:
        surya_version = importlib.metadata.version("surya-ocr")
    except Exception:
        surya_version = "unknown"

    emit(
        "runner_start",
        marker_version=marker_version,
        surya_version=surya_version,
        file_count=len(files),
    )

    install_surya_progress_patch()

    from marker.config.parser import ConfigParser
    from marker.logger import configure_logging, get_logger
    from marker.models import create_model_dict, shutdown_models
    from marker.output import save_output

    configure_logging()
    logger = get_logger()

    cli_options = parse_marker_options(
        files[0],
        marker_args,
        output_dir,
    )

    config_parser = ConfigParser(cli_options)
    converter_class = config_parser.get_converter_cls()
    base_config = config_parser.generate_config_dict()
    processor_list = config_parser.get_processors()
    renderer = config_parser.get_renderer()
    llm_service = config_parser.get_llm_service()

    os.makedirs(output_dir, exist_ok=True)

    emit("models_start")
    model_dict = create_model_dict()
    emit("models_ready")

    ok_count = 0
    fail_count = 0

    try:
        for index, file_path in enumerate(files, start=1):
            started = time.monotonic()
            converter = None
            rendered = None

            emit(
                "file_start",
                index=index,
                total=len(files),
                file=file_path,
            )

            try:
                if not os.path.isfile(file_path):
                    raise FileNotFoundError(file_path)

                # Create one converter per file, while sharing the Surya model
                # clients/server across the whole batch.
                converter = converter_class(
                    config=dict(base_config),
                    artifact_dict=model_dict,
                    processor_list=processor_list,
                    renderer=renderer,
                    llm_service=llm_service,
                )

                rendered = converter(file_path)

                output_folder = (
                    config_parser.get_output_folder(file_path)
                )
                base_name = (
                    config_parser.get_base_filename(file_path)
                )

                save_output(
                    rendered,
                    output_folder,
                    base_name,
                )

                elapsed = time.monotonic() - started
                page_count = getattr(
                    converter,
                    "page_count",
                    None,
                )

                logger.info(
                    "Saved output to %s",
                    output_folder,
                )

                emit(
                    "file_done",
                    index=index,
                    total=len(files),
                    file=file_path,
                    status="OK",
                    seconds=elapsed,
                    pages=page_count,
                    output_folder=output_folder,
                )

                ok_count += 1

            except Exception as exc:
                elapsed = time.monotonic() - started

                print(
                    "Conversion failed: "
                    + file_path
                    + ": "
                    + str(exc),
                    flush=True,
                )
                traceback.print_exc()

                emit(
                    "file_done",
                    index=index,
                    total=len(files),
                    file=file_path,
                    status="FAIL",
                    seconds=elapsed,
                    error=str(exc),
                )

                fail_count += 1

            finally:
                try:
                    del rendered
                except Exception:
                    pass

                try:
                    del converter
                except Exception:
                    pass

                gc.collect()

    finally:
        try:
            shutdown_models(model_dict)
        except Exception:
            pass

    emit(
        "runner_done",
        ok=ok_count,
        fail=fail_count,
    )

    return 1 if fail_count else 0


def main():
    redirect_stderr_to_stdout()

    if len(sys.argv) != 2:
        print(
            "Usage: MarkerProgress.py job.json",
            flush=True,
        )
        return 2

    try:
        return run_job(sys.argv[1])
    except Exception as exc:
        print(
            "Fatal runner error: " + str(exc),
            flush=True,
        )
        traceback.print_exc()

        emit(
            "runner_fatal",
            error=str(exc),
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())