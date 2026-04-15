# Future Work: Proving `_Thread_Dispatch_necessary`

## Background

The ensures `_Thread_Dispatch_necessary == true` in the
`exec_update_new_h` behavior of `_Scheduler_EDF_Update_priority` (and
the equivalent in `_Scheduler_EDF_Unblock`) cannot currently be proved.

The root cause is that `dispatch_necessary` is declared `volatile bool`
in `percpu.h:400`. WP correctly refuses to prove postconditions about
volatile writes — the C standard says a volatile variable may change at
any time by external means, so a write cannot be assumed to persist at
function exit.

The property matters: `_Thread_Dispatch_necessary` is the flag that
triggers the context switch. Without it, the scheduler would select a
new heir but the system would keep running the old thread.

The code does set it (`schedulerimpl.h:1225`:
`_Thread_Dispatch_necessary = true;`), and in practice the value
persists because scheduler operations run inside ISR-disabled critical
sections on a uniprocessor — no interrupt handler can modify it
mid-function.

## Approach: Frama-C Volatile Model Plugin

Instead of stripping `volatile` (which silently strengthens the
verification assumptions) or leaving the ensures commented out (which
leaves a gap), Frama-C provides a volatile model plugin that lets you
define explicit semantics for volatile accesses.

### How it works

In ACSL, you declare model functions for volatile reads and writes:

```
//@ volatile location reads read_func writes write_func;
```

Every volatile read of `location` is replaced by a call to
`read_func()`, and every write `location = v` by a call to
`write_func(v)`. WP uses the contracts of these model functions for
proof obligations.

### What it would look like

```c
/*@ assigns \result \from
      _Per_CPU_Information[0].per_cpu.dispatch_necessary;
    ensures \result ==
      _Per_CPU_Information[0].per_cpu.dispatch_necessary;
*/
bool read_dispatch_necessary(void);

/*@ assigns _Per_CPU_Information[0].per_cpu.dispatch_necessary
      \from v;
    ensures _Per_CPU_Information[0].per_cpu.dispatch_necessary == v;
*/
void write_dispatch_necessary(bool v);

//@ volatile _Per_CPU_Information[0].per_cpu.dispatch_necessary
//@   reads read_dispatch_necessary
//@   writes write_dispatch_necessary;
```

The write model contract says "after writing `v`, the location holds
`v`" — the "writes persist" assumption. With this, WP could prove
`ensures _Thread_Dispatch_necessary == true`.

The advantage over stripping `volatile` is that the assumption is
explicit and auditable. A reviewer can examine the model function
contracts and decide whether they are sound for the given context
(uniprocessor, critical section, interrupts disabled).

### Open questions

1. **Struct field support.** The `volatile` qualifier is on a struct
   field (`Per_CPU_Control.dispatch_necessary`), accessed through a
   pointer chain (`&_Per_CPU_Information[0].per_cpu`). The Frama-C
   volatile plugin is designed primarily for simple global volatile
   variables. Whether it handles volatile struct fields accessed through
   pointer indirection in Frama-C 25 needs testing.

2. **Macro indirection.** `_Thread_Dispatch_necessary` is a macro
   expanding to `_Per_CPU_Get()->dispatch_necessary`. The volatile
   annotation must target the resolved lvalue, not the macro name. The
   annotation would likely go in the stubs file, after the RTEMS headers
   are included.

3. **Plugin availability.** The volatile plugin may need explicit
   loading (`-load-module volatile`). Its status in Frama-C 25
   (Manganese) needs verification.

4. **Interaction with `Typed+Cast`.** The volatile model introduces
   additional proof context. Whether this interacts cleanly with the
   `Typed+Cast` memory model that the rest of the verification relies on
   is unknown.

5. **Scope of the assumption.** The "writes persist" model is sound
   here because the verified scheduler functions run inside
   ISR-disabled critical sections — interrupts cannot fire between
   the write and the function exit. The `volatile` only matters for
   the subsequent read at the dispatch check site, which is outside
   the current verification scope.

### How to test

A minimal experiment would be:

1. Add the volatile annotation and model functions to `stubs.h`
2. Remove `volatile` from the `dispatch_necessary` field declaration
   (so the ACSL volatile model takes over)
3. Uncomment all three `ensures _Thread_Dispatch_necessary == true`
   (in `_Scheduler_Update_heir`, `_Scheduler_EDF_Schedule_body`,
   and `_Scheduler_EDF_Update_priority`)
4. Run WP on `_Scheduler_Update_heir` in isolation first
5. If that works, test the full inlined chain via
   `_Scheduler_EDF_Update_priority`

If step 1 fails due to the struct-field-through-pointer issue, a
fallback is to introduce a non-volatile global `bool` mirror variable
set by a wrapper, and annotate that instead.
