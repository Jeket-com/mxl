<!-- SPDX-FileCopyrightText: 2026 Contributors to the Media eXchange Layer project. -->
<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Proposal: provisioned flows — a flow that exists before, and outlives, its writer

*Status: draft. Not yet submitted to the MXL Requirements Council.*

## Problem

Today a flow exists only while a writer holds it.

- `mxlCreateFlowWriter` creates the flow, or opens it if it exists
  (`created = false`).
- Writers hold a shared `flock`. The last writer to release deletes the flow if
  it can take the lock exclusively. `~Instance` deletes the flows of writers the
  process leaked.
- `mxlCreateFlowReader` fails with `MXL_ERR_FLOW_NOT_FOUND` until a writer has
  created the flow.
- A reader whose flow is deleted and recreated sees `MXL_ERR_FLOW_INVALID` (the
  inode changed) and must reopen it.

For a runtime that is the BCP-007-03 controller for a whole domain, this has
four consequences:

1. **Start-up is ordered.** Every writer must be up before its readers can
   connect, so a graph comes up one stage at a time instead of all at once.
2. **A writer restart breaks every reader.** The flow is deleted and recreated,
   and every reader has to notice and reopen it.
3. **"Waiting for a source" looks like "broken".** A receiver can't be connected
   to a flow whose writer hasn't started, so the controller has no state for
   "connected, no writer yet".
4. **The flow can't be set up ahead of its sender.** BCP-007-03 already lets a
   controller stage a Sender's `mxl_flow_id` before `master_enable`, which
   implies a flow that exists before its writer starts.

## What we do today, and why it is fragile

A small per-host agent calls `mxlCreateFlowWriter` for each flow the runtime
provisions and never commits a grain. Its shared lock keeps the flow alive
while the real writer comes and goes. Readers connect at once and see a frozen
head until the writer starts.

This works, but it depends on behaviour nobody has specified:

- **No definition check on reopen (#670).** A writer whose flow definition has
  a different geometry silently opens the existing ring.
- **No single-writer rule.** The anchor and the real writer are both writers,
  and nothing stops a third.
- **"A writer is present" stops meaning "media is flowing."** The anchor counts
  as a writer.
- **GC is unspecified for held flows.** It is unclear how
  `mxlGarbageCollectFlows` (explicit-only since #527) should treat a flow held
  by something that is not producing.

## Proposal (API sketch, open to the maintainers' shape)

1. **`mxlProvisionFlow(instance, flowDef, options, &handle, &configInfo)`.**
   - Creates the flow in the domain with no writer. The handle holds the flow
     until `mxlReleaseProvisionedFlow(handle)`, or until `mxlDeleteFlow` is
     called on it.
   - A later `mxlCreateFlowWriter` opens the flow (`created = false`).
   - That call fails with a new `MXL_ERR_FLOW_DEFINITION_MISMATCH` when the
     writer's definition differs in anything that fixes the layout: format,
     rate, geometry, or grain/slice sizes.
2. **Exclusive writer, as an option.** With `MXL_WRITER_EXCLUSIVE`, a second
   concurrent writer is refused (`MXL_ERR_FLOW_BUSY`). Hand-over then means
   releasing the first writer before opening the next.
3. **Writer presence that readers can see.** Runtime info exposes whether an
   active writer is attached, and when the last one detached. A reader can then
   tell "idle" from "broken" without guessing from timeouts.
4. **Lifecycle.**
   - When the last writer releases a provisioned flow, the flow is not deleted.
     Only its holder, or an explicit `mxlDeleteFlow`, deletes it.
   - GC skips flows that have a holder.
5. **Compatibility.**
   - The change is additive: flows that were not provisioned keep today's
     create-or-open and last-writer-deletes behaviour.
   - How the holder is recorded (a lock of its own, or a field in the flow's
     shared state) is the maintainers' call.

## Questions for the Requirements Council

- Should this be a separate API, or a flag on `mxlCreateFlowWriter` ("open
  only, never create" plus "do not delete on release")?
- Should the definition check on reopen (#670) land on its own, ahead of
  this?
- How does it interact with fabrics targets, which already create replica flows
  from a definition, and with event flows (#798)?
