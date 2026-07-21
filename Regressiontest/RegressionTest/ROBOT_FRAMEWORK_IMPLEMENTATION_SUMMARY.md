# Robot Framework implementation summary

Version **2.7.0** exposes four Robot suites:

| Profile | Suite | Tests |
|---|---|---:|
| Read-only | `read_only.robot` | 18 |
| Safe-output | `safe_output.robot` | 21 |
| Single-channel HIL | `hil_single_channel.robot` | 26 |
| Gate 3 fault injection | `gate3_transport_fault.robot` | 7 |

Total static test definitions: **72**.

The Robot library delegates protocol, DMM, serial, evidence, reporting, and mathematical checks to internal Python implementation modules. No standalone pure-Python regression runner is exposed. Every Robot test maps one test ID to one `RegressionSuite` method, then records detailed metrics and evidence before converting the result to Robot PASS/FAIL/SKIP status.
