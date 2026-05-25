This is a verification of UP EDF scheduling in RTEMS 6.2/5.1. Focus lies on EDF API functions.
RBTree operations are out of scope. The wrapper functions for it are given contracts and not verified.
SMP is out of scope.

Use docker compose to run the verification. This is a beefy machine. If a timeout of 30 seconds is not sufficient, it's very unlikely a higher timeout will help.
Do not modify the actual C code.
