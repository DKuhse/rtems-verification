# Release Job to Priority Update Chain

This note records the current boundary decision for the EDF release/cancel job
work.

The project scope is the EDF scheduler contract chain:

```c
_Scheduler_Release_job(owner, priority_node, deadline, &queue_context);
_Thread_Priority_update(&queue_context);

_Scheduler_Cancel_job(owner, priority_node, &queue_context);
_Thread_Priority_update(&queue_context);
```

The dispatch, watchdog, rate-monotonic locking, and timer insertion paths are
outside this verification effort. They may have contracts and may be assumed
correct by callers of the EDF scheduler slice.

## Current Observation

The verified `_Scheduler_EDF_Release_job()` and `_Scheduler_Release_job()`
contracts appear strong enough for the release side of the chain:

- the job priority node is assigned from the deadline;
- the priority aggregation remains well formed and has a cached minimum;
- the EDF ready set is preserved;
- EDF ready-context well formedness is preserved;
- when the thread scheduler-node priority changes, the thread is recorded in
  the priority-update queue context.
- in the active EDF release branch, if the thread wait operations use
  `_Thread_queue_Do_nothing_priority_actions`, the priority action list is
  drained. This records the non-priority-inheritance subcase expected for the
  release/cancel slice without claiming it for all thread-priority users. The
  same conditional fact is exposed by the generic `_Scheduler_Release_job()`
  wrapper, which is the intended boundary for the release/update composition
  proof.
- the same no-op conditional is exposed for the inactive EDF release branch:
  the job priority node is inserted into the thread scheduler-node wait
  priority aggregation and the priority action list is drained.
- the matching no-op conditional is exposed for active EDF cancel through
  `_Scheduler_EDF_Cancel_job()` and `_Scheduler_Cancel_job()`: the job priority
  node is extracted from the thread scheduler-node wait priority aggregation
  and the priority action list is drained.

The intended follow-up property is therefore not an end-to-end proof of the
rate-monotonic release/cancel path. It is a small composition proof showing
that the scheduler release/cancel contract produces the queue-context state
consumed by `_Thread_Priority_update()`, and that update restores/preserves the
EDF ready context while leaving the modeled ready set unchanged.

## Out of Scope for This Check

The previous `_Rate_monotonic_Release_job()` probe mostly exposed unrelated
framing obligations from watchdog insertion, dispatch-disable volatile state,
and lock release. Those obligations did not indicate a weakness in the EDF
release/update contracts. They should be handled by assumed contracts if a
higher-level RM proof is attempted later.
