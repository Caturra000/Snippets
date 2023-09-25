# 这是我的静态博客（jekyll）用到的makefile，都是一些便捷命令，和构建无关
#
# Jekyll APIs:
# $ make
# $ make cached
# $ make template <title>
# $ make push


# Just run jekyll server
# Usage: make
PHONY = default
default:
	@${ASYNC_RM}
	@${BUILD_OPT}

# Just run jekyll server faster
#
# NOTE: explicitly show build messages
# TODO: make fff (super fast, no katex rendering)
#
# Usage: make cached
PHONY += cached
cached:
	@${ASYNC_RM}
	${BUILD_OPT} --incremental

# Generate a template for posts
# Usage: make template <title>
# Example: make template jintianxiaomidaobilema
#
# TODO:
# - ignore the up-to-date message
# - make dir
PHONY += template
template:
	@touch ${DEST_FILENAME}
	@echo --- >> ${DEST_FILENAME}
	@echo layout: post >> ${DEST_FILENAME}
	@echo title: ${TEMP_TITLE} >> ${DEST_FILENAME}
	@echo categories: >> ${DEST_FILENAME}
	@echo tags: >> ${DEST_FILENAME}
	@echo --- >> ${DEST_FILENAME}
	@echo '\n## Description\n\n## Constraints\n\n## Solution\n\n## Code\n\n## Link' >> ${DEST_FILENAME}
	@echo done.

# Push for unproxied network
# Usage: make push
PHONY += push
push:
	@for _ in `seq 10`; do \
		timeout 15s git push; \
	done;

# Did-nothing-wrong action
# Match anything and ignore
# @: a nop recipe
%:
	@:


# A blocking command
BUILD_OPT = bundle exec jekyll serve
# Call before BUILD_OPT
ASYNC_RM = (sleep 1 && rm Gemfile.lock) &


# Format: 1926-08-17
DATE = ${shell date -Idate}
# 1926
YEAR = ${shell date "+%Y"}


# Tricks:
# - $@: action name
# - `filter-out a, b`: filter a from b
# - `${MAKECMDGOALS}`: arguments and action name
TEMP_TITLE = ${filter-out $@,${MAKECMDGOALS}}
FILENAME = ${DATE}-${TEMP_TITLE}.md
DEST_FILENAME = _posts/${YEAR}/${FILENAME}

.PHONY: ${PHONY}
