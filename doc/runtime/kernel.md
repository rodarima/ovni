# Kernel support

Currently, only context switch events are supported. The kernel events are
usually written by the kernel into a buffer, without any action from user space.
This behavior poses a problem, as the user space events and kernel events can
leave a unsorted trace.

The current workaround involves surounding the kernel events by two special ovni
event markers `OU[` and `OU]` which determine the region of events which must be
sorted first. Notice that the events inside the region must be sorted!

The `ovnisort` tool has been designed to sort the events enclosed by those
markers by using a very simple window sorting algorithm, trying to insert them
in order by looking only at the past 10000 events.

To use the kernel events, you must sort the ovni trace before calling the
emulator:

	% ./application
	% ovnisort ovni
	% ovniemu ovni
