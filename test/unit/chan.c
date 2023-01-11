#include "emu/chan.h"
#include "common.h"

static void
test_dirty(void)
{
	struct chan chan;
	chan_init(&chan, CHAN_SINGLE, "testchan");

	if (chan_set(&chan, value_int64(1)) != 0)
		die("chan_set failed\n");

	/* Now channel is dirty */

	if (chan_set(&chan, value_int64(2)) == 0)
		die("chan_set didn't fail\n");

	if (chan_flush(&chan) != 0)
		die("chan_flush failed\n");

	chan_prop_set(&chan, CHAN_DIRTY_WRITE, 1);

	if (chan_set(&chan, value_int64(3)) != 0)
		die("chan_set failed\n");

	/* Now is dirty, but it should allow multiple writes */
	if (chan_set(&chan, value_int64(4)) != 0)
		die("chan_set failed\n");

	struct value value;
	struct value four = value_int64(4);

	if (chan_read(&chan, &value) != 0)
		die("chan_read failed\n");

	if (!value_is_equal(&value, &four))
		die("chan_read returned unexpected value\n");

	if (chan_flush(&chan) != 0)
		die("chan_flush failed\n");
}

static void
test_single(void)
{
	struct chan chan;
	struct value one = { .type = VALUE_INT64, .i = 1 };
	struct value two = { .type = VALUE_INT64, .i = 2 };
	//struct value nil = { .type = VALUE_NULL, .i = 0 };

	chan_init(&chan, CHAN_SINGLE, "testchan");

	/* Ensure we cannot push as stack */
	if (chan_push(&chan, one) == 0)
		die("chan_push didn't fail\n");

	/* Now we should be able to write with set */
	if (chan_set(&chan, one) != 0)
		die("chan_set failed\n");

	/* Now is dirty, it shouldn't allow another set */
	if (chan_set(&chan, two) == 0)
		die("chan_set didn't fail\n");

	struct value value;

	if (chan_read(&chan, &value) != 0)
		die("chan_read failed\n");

	if (!value_is_equal(&value, &one))
		die("chan_read returned unexpected value\n");
}

static void
test_duplicate(void)
{
	struct chan chan;
	chan_init(&chan, CHAN_SINGLE, "testchan");

	if (chan_set(&chan, value_int64(1)) != 0)
		die("chan_set failed\n");

	if (chan_flush(&chan) != 0)
		die("chan_flush failed\n");

	/* Attempt to write the same value again */
	if (chan_set(&chan, value_int64(1)) == 0)
		die("chan_set didn't fail\n");

	/* Now enable duplicates */
	chan_prop_set(&chan, CHAN_DUPLICATES, 1);

	/* Then it should allow writting the same value */
	if (chan_set(&chan, value_int64(1)) != 0)
		die("chan_set failed\n");
}

//static void
//test_stack(void)
//{
//	struct chan chan;
//
//	chan_init(&chan, CHAN_STACK);
//
//	/* Ensure we cannot set as single */
//	if (chan_set(&chan, 1) == 0)
//		die("chan_set didn't fail\n");
//
//	/* Channels are closed after init */
//	if (chan_push(&chan, 1) != 0)
//		die("chan_push failed\n");
//
//	/* Now is closed, it shouldn't allow another value */
//	if (chan_push(&chan, 2) == 0)
//		die("chan_push didn't fail\n");
//
//	struct chan_value value = { 0 };
//
//	if (chan_flush(&chan, &value) != 0)
//		die("chan_flush failed\n");
//
//	if (!value.ok || value.i != 1)
//		die("chan_flush returned unexpected value\n");
//
//	/* Now it should allow to push another value */
//	if (chan_push(&chan, 2) != 0)
//		die("chan_push failed\n");
//
//	if (chan_flush(&chan, &value) != 0)
//		die("chan_flush failed\n");
//
//	if (!value.ok || value.i != 2)
//		die("chan_flush returned unexpected value\n");
//
//	/* Now pop the values */
//	if (chan_pop(&chan, 2) != 0)
//		die("chan_pop failed\n");
//
//	if (chan_flush(&chan, &value) != 0)
//		die("chan_flush failed\n");
//
//	if (!value.ok || value.i != 1)
//		die("chan_flush returned unexpected value\n");
//
//	if (chan_pop(&chan, 1) != 0)
//		die("chan_pop failed\n");
//
//	/* Now the stack should be empty */
//
//	if (chan_pop(&chan, 1) == 0)
//		die("chan_pop didn't fail\n");
//
//}

int main(void)
{
	test_single();
	test_dirty();
	test_duplicate();
	//test_stack();
	return 0;
}