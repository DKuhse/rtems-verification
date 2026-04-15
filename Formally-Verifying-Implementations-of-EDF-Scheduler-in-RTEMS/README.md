# Formal Verification of EDF Scheduler in RTEMS
This repository contains the files to reproduce the results of our work:
_Formally Verifying Implementations of Earliest Deadline First Scheduler in RTEMS_

- [Enviroment Setup](#enviroment-setup)
  * [Frama-C](#frama-c)
    + [Installation](#installation)
    + [Invoking Frama-C GUI](#invoking-frama-c-gui)
  * [RTEMS 5.1](#rtems-51)
    + [2.4 Install the Tool Suite:](#24-install-the-tool-suite-)
    + [2.6.2 Manual BSP Build:](#262-manual-bsp-build-)
  * [Source Code Modification to make it run with Frama-C](#source-code-modification-to-make-it-run-with-frama-c)
  * [Copy the files of this repository to the following locations](#copy-the-files-of-this-repository-to-the-following-locations)
- [Functionlist](#functionlist)
  * [Thread Priority](#thread-priority)
  * [EDF Releasing and Cancelling a Job](#edf-releasing-and-cancelling-a-job)
  * [EDF Update Priority](#edf-update-priority)
  * [EDF Unblock](#edf-unblock)

## Enviroment Setup
The contracts are tested with Frama-C 25 and Alt-Ergo 2.4.2 <br>
RTEMS 5.1 is needed for verification.

### Frama-C

#### Installation
Follow the instructions to install Frama-C https://frama-c.com/html/get-frama-c.html:
1. Install opam (OCaml package manager): Use the package manager based on your distribution

2. Initialize opam (install an OCaml compiler)
```
opam init --compiler 4.14.1 # may take a while
eval $(opam env)
```

3. Install Frama-C (and its dependencies):
```
opam install frama-c
```

4. Configuring provers for Frama-C/WP
```
why3 config detect
```

#### Invoking Frama-C GUI
- Please see the examples under the [Functionlist](#functionlist) section.


### RTEMS 5.1
Follow the instructions here to setup RTEMS 5.1: https://ftp.rtems.org/pub/rtems/releases/5/5.1/docs/html/user.html#document-start/index
Installation files: https://ftp.rtems.org/pub/rtems/releases/5/5.1/

#### 2.4 Install the Tool Suite:
For our case, we need the x86_64 tool suite. The name of the architecture for it is rtems-x84_64.
Example (replace it with your local paths):
```
../source-builder/sb-set-builder --prefix=$HOME/RTEMS/rtems_x86_64 5/rtems-x86_64
```
#### 2.6.2 Manual BSP Build:
Use step 2.6.2 instead and do a manual BSP build. Create the corresponding folders like in the tutorial.
Our target BSP is AMD64, change the directory to the one you created and configute the builder (remember to export your PATH):
```
$HOME/RTEMS/src/rtems-5.1/configure \
    --prefix=$HOME/RTEMS/rtems_x86_64 \
    --enable-maintainer-mode \
    --target=x86_64-rtems5 \
    --enable-rtemsbsp=amd64
```
```
make
make install
```
### Source Code Modification to make it run with Frama-C
- Comment out limits.h in cpu/include/rtems/score/scheduleredf.h:29
- Comment out line 883 in cpu/include/rtems/score/thread.h:883
- Replace Replace line 620 in cpu/include/rtems/score/percpu.h:620 with ```Per_CPU_Control_envelope _Per_CPU_Information[1U];```

### Copy the files of this repository to the following locations
- Both stub files: cpukit directory
- other headerfiles: cpukit/include/rtems/score
- c files: cpukit/score/src
```
├── build
│   └── amd64 
├── rtems_x86_64
├── src
|   ├── rtems-5.1
|   |   ├── cpukit
|   |   |   ├── stubs.h
|   |   |   ├── release_and_cancel_stubs.h
|   |   |   ├── score
|   |   |   |   └── src
|   |   |   |       ├── scheduleredfchangepriority.c
|   |   |   |       ├── scheduleredfreleasejob.c
|   |   |   |       ├── scheduleredfunblock.c
|   |   |   |       └── threadchangepriority.c
|   |   |   └── include
|   |   |       └── rtems
|   |   |           └── score
|   |   |               ├── priorityimpl.h
|   |   |               ├── scheduleredfimpl.h
|   |   |               └── schedulerimpl.h
|   |   └── (...)
```


## Functionlist
For verifying the functions, select the corresponding function on the left side of the GUI. In the following is a list with all the relevant annontated functions.
Additionally, example commands are provided to invoke the verification with Frama-C.

### Thread Priority
```
frama-c-gui       -cpp-command '$HOME/RTEMS/rtems_x86_64/bin/x86_64-rtems5-gcc -C -E \
      -I./include -I./score/cpu/x86_64/include/ \
      -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
      -I$HOME/RTEMS/rtems_x86_64/x86_64-rtems5/include \
      -I$HOME/RTEMS/rtems_x86_64/lib/gcc/x86_64-rtems5/9.3.0/include \
      -nostdinc -include stubs.h' -machdep gcc_x86_64 -cpp-frama-c-compliant -c11  \
      -inline-calls '_Priority_Node_is_active,_Priority_Extract_non_empty, _Priority_Non_empty_insert, _Priority_Changed' 'score/src/threadchangepriority.c'
```
For the _Thread_Priority_apply function add _Thread_Priority_do_perform_actions to the inline functions under Analysis->Kernel->Customizing Normalization in the GUI <br>
For the ..._add,remove and changed function, add the _Thread_Priority_apply in addition to the perform_actions function above, for the assigns to work.
 
  - cpukit/score/src/threadchangepriority.c
       - _Thread_Set_scheduler_node_priority (*)
       - _Thread_Priority_action_change (*)
       - _Thread_Priority_do_perform_actions (*)
       - _Thread_Priority_apply (*)
       - _Thread_Priority_add (*)
       - _Thread_Priority_remove (*)
       - _Thread_Priority_changed (*)
  - cpukit/include/rtems/score/priorityimpl.h
       - _Priority_Actions_move
       - _Priority_Get_next_action        
       - _Priority_Non_empty_insert (*)
       - _Priority_Extract_non_empty (*)
       - _Priority_Changed (*)
       - _Priority_Actions_is_empty
       - _Priority_Actions_add
      - _Priority_Actions_initialize_one
       - _Priority_Get_priority
   - cpukit/include/rtems/score/threadqimpl.h
       - _Thread_queue_Context_add_priority_update (*)
   - cpukit/include/rtems/score/schedulernodeimpl.h:
       - _Scheduler_Node_set_priority
   - stubs:
       - _Helper_RBTree_Minimum
       - _Thread_queue_Do_nothing_priority_actions

### EDF Update Priority
```
frama-c-gui       -cpp-command '$HOME/RTEMS/rtems_x86_64/bin/x86_64-rtems5-gcc -C -E \
      -I./include -I./score/cpu/x86_64/include/ \
      -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
      -I$HOME/RTEMS/rtems_x86_64/x86_64-rtems5/include \
      -I$HOME/RTEMS/rtems_x86_64/lib/gcc/x86_64-rtems5/9.3.0/include \
      -nostdinc -include stubs.h' -machdep gcc_x86_64 -cpp-frama-c-compliant -c11  \
      -inline-calls '_Scheduler_Update_heir, _Scheduler_EDF_Schedule_body' 'score/src/scheduleredfchangepriority.c'
 ```
   - cpukit/score/src/scheduleredfchangepriority.c
       - _Scheduler_EDF_Update_priority (*)
   - cpukit/include/rtems/score/threadimpl.h
       - _Thread_Is_ready
   - cpukit/include/rtems/score/scheduleredfimpl.h
       - _Scheduler_EDF_Node_downcast
       - _Scheduler_EDF_Get_context
       - _Scheduler_EDF_Schedule_body (*)
   - cpukit/include/rtems/score/schedulernodeimpl.h
       - _Scheduler_Node_get_priority
   - cpukit/include/rtems/score/schedulerimpl.h
       - _Scheduler_Update_heir (*)
       
### EDF Unblock
```
frama-c-gui       -cpp-command '$HOME/RTEMS/rtems_x86_64/bin/x86_64-rtems5-gcc -C -E \
      -I./include -I./score/cpu/x86_64/include/ \
      -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
      -I$HOME/RTEMS/rtems_x86_64/x86_64-rtems5/include \
      -I$HOME/RTEMS/rtems_x86_64/lib/gcc/x86_64-rtems5/9.3.0/include \
      -nostdinc -include stubs.h' -machdep gcc_x86_64 -cpp-frama-c-compliant -c11  \
      -inline-calls '_Scheduler_Update_heir' 'score/src/scheduleredfunblock.c'
```
   - cpukit/score/src/scheduleredfunblock.c
       - _Scheduler_EDF_Unblock (*)
  
### EDF Releasing and Cancelling a Job
```
frama-c-gui       -cpp-command '$HOME/RTEMS/rtems_x86_64/bin/x86_64-rtems5-gcc -C -E \
      -I./include -I./score/cpu/x86_64/include/ \
      -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
      -I$HOME/RTEMS/rtems_x86_64/x86_64-rtems5/include \
      -I$HOME/RTEMS/rtems_x86_64/lib/gcc/x86_64-rtems5/9.3.0/include \
      -nostdinc -include release_cancel_stubs.h' -machdep gcc_x86_64 -cpp-frama-c-compliant -c11  \
      'score/src/scheduleredfreleasejob.c'
```
  - cpukit/score/src/scheduleredfreleasejob.c
       - _Scheduler_EDF_Release_job (*)
       - _Scheduler_EDF_Cancel_job (*)
       - _Scheduler_EDF_Map_priority
       - _Scheduler_EDF_Unmap_priority
  - cpukit/include/rtems/score/priorityimpl.h
       - _Priority_Node_is_active
       - _Priority_Node_set_inactive
       - _Priority_Node_set_priority


