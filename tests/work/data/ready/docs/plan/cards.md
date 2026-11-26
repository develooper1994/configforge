## R-0 - A card that is done

* **id**: `R-0`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.8.0
* **status**: done (abc1234)
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-1 - The ready card

* **id**: `R-1`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: R-0
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * A `TODO` or `a?b` inside backticks is code; `cf_one_options` has a default.
  * It reads `src/code.c:5-8` (`widget_open`, the first function) and `widget_shut()` at `src/code.c:6`.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.
* **notes**: 2026-11-27: a TODO and a question? either way.

## R-2 - No docs field

* **id**: `R-2`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.

## R-3 - Not open

* **id**: `R-3`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: parked
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-4 - Undecided

* **id**: `R-4`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * Names: TBD.
  * Wording: to be rewritten later.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-5 - Unsettled

* **id**: `R-5`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * Is it a string?
  * Either way works.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-6 - No default

* **id**: `R-6`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * `typedef struct cf_two_options { int n; } cf_two_options;` is a struct; `cf_three_options` is only named.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-7 - Beyond the file

* **id**: `R-7`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * See `src/code.c:30-60`.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-8 - Wrong place

* **id**: `R-8`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * `widget_open` is at `src/code.c:20-22`.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-9 - Missing file

* **id**: `R-9`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * See `src/none.c:1-2`.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-10 - Needs an open card

* **id**: `R-10`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: R-11
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-11 - The open card

* **id**: `R-11`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-12 - Needs a later version

* **id**: `R-12`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: R-13
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-13 - A later card

* **id**: `R-13`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 2.0.0
* **status**: done (def5678)
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.

## R-14 - Title (TO DECIDE)

* **id**: `R-14`
* **goal**: Something is true.
* **why**: A reason.
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: open
* **decided**:
  * One choice, made.
* **files**: `src/code.c`
* **steps**: 1. Do it.
* **accept**: ACCEPT-LINUX.
* **stop if**: It fails.
* **docs**: None.
