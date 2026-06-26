# TVMS Rogue DGN controlled-capture matrix

The existing object and CAN captures fully map the configuration transport and the object-derived portions of `1F108`, `1FD04`, `1FD06`, `1FD0A`, and `1FD0E`. The following tests are required to name the remaining state enums without guessing.

| Test | Change/action | DGNs to record | Result needed |
|---|---|---|---|
| Input profile | Change only input 8 between 12/24 V signal, push-button NO and push-button NC | `1FD10`, object 2 | Determine whether `1FD10` carries electrical profile or another secondary property |
| Sensor validity | Disconnect/reconnect tank 2, then voltage input, then current input | `1FD07`, `1FD02` | Name `FC`, `FD`, `FE`, `FF` validity/status values |
| Tank sender profile | Change tank 2 through TLSEN175, TLSEN200, TLSEN225 and Custom | `1FD0C`, `1FD06`, object 2 | Name engineering-format codes and identify profile-dependent range fields |
| Channel inventory | Disable one input, one tank and one remote channel independently | `1FD08`, `1FD0A` | Decode bytes D2-D8 of `1FD08` |
| Output activity | Hold-dim one output up/down, release, force transient/override and provoke a protected/fault state if safe | `1FD14`, `1FD00` | Extend confirmed `00=idle`, `02=hold-ramp active`; name override/fault states |

Each capture should start from a known saved object and change one field only. Record the complete write, immediate configuration readback and at least five seconds of runtime broadcasts before and after the change.
