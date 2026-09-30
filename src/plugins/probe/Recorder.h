// The probe's recorder: a thread of its own that writes two files beside
// probe.log, both tab-separated, with times in milliseconds since the probe
// loaded (the first line of each gives the local time that is).
//
// probe-calls.tsv, once a second:
//   <ms>  status  <paused 0/1/-1>  <mode, -1 unknown>  <second flag 0/1/-1>
//   <ms>  call    <address>  <calls in the last second>  <label>
// the census's counts (Census.h) for every target called in that second,
// and the application's pause state and game mode (dao/ClientExoApp.h).
//
// probe-samples.tsv, one block per ten-second window:
//   <from>  <to>  samples  0          <thread samples taken>
//   <from>  <to>  thread   <tid>      <samples with the thread in the exe's code>
//   <from>  <to>  eip      <address>  <samples with a thread there>
//   <from>  <to>  stack    <address>  <samples with it on a thread's stack>
// Every thread of the process but the recorder's own is sampled about every
// 15 ms: suspended, its registers and up to 16 KiB of stack copied, resumed.
// An eip row says where threads were, the leaves; a stack row is a return
// address (dao/core/CallSite.h) found in the copy, so a caller. tools/
// probe_report.py maps both to functions and names.
#pragma once

namespace probe
{

// Starts the thread. False if it could not be started or the exe's code
// section could not be found; the reason is in probe.log.
bool StartRecorder();

} // namespace probe
