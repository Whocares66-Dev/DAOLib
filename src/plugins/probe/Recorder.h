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
//   <from>  <to>  usamples 0          <samples with the game's update on the stack>
//   <from>  <to>  ueip     <address>  <those samples with a thread there>
//   <from>  <to>  ustack   <address>  <those samples with it on the stack>
// The u-rows are the game's logic alone: samples of the threads while they
// ran Eclipse::UpdateTask (dao/Addresses.h), not while they idled or drew.
// Every thread of the process but the recorder's own is sampled about every
// 15 ms: suspended, its registers and up to 16 KiB of stack copied, resumed.
// An eip row says where threads were, the leaves; a stack row is a return
// address (dao/core/CallSite.h) found in the copy, so a caller. tools/
// probe_report.py maps both to functions and names.
//
// probe-loading.tsv, one block per loading screen (DAOLib #68): ten seconds
// of every thread say nothing of a loading screen, which is a second or two
// of one thread. While the loading watch (Loading.h) says a screen is up,
// the thread that runs the loading mode's update is sampled about every
// millisecond, and its samples kept by the mode's state and step, apart for
// those taken inside the update and between two of them:
//   <n>  screen    <from ms>  <to ms>  <samples of the loading thread>
//   <n>  cpu       <tid>  <ms of processor time over the screen>  <1 for the loading thread>  <its name, or ?>
//   <n>  step      <state>  <step>  <inside 0/1>  <samples>  <of them outside the exe's code>
//   <n>  leaf      <state>  <step>  <inside>  <address>  <samples with the thread there>
//   <n>  inner     <state>  <step>  <inside>  <address>  <samples outside the exe that returned here first>
//   <n>  stack     <state>  <step>  <inside>  <address>  <samples with it on the stack>
//   <n>  module    <state>  <step>  <inside>  <module!export>  <samples outside the exe with the thread there:
//                  the module's export at or before it, which for code between two exports is the earlier one>
//   <n>  via       <state>  <step>  <inside>  <a < b < c>  <samples outside the exe whose stack, from its top,
//                  returned through these: module!export, or the game's own address; the innermost first>
//   <n>  chain     <state>  <step>  <inside>  <a,b,c,d>  <samples whose first four callers were these, innermost first>
//   <n>  health    <turns of the recorder>  <longest gap between two, ms>  <suspends failed>  <contexts failed>
//                  <turns with the loading thread unknown>
// and the same step rows with a thread's name and a dot before the kind
// (DragonAge::Render.step, DragonAge::Streaming.leaf and so on) for each of
// the engine's threads watched with the loading thread: the render thread,
// the streaming thread and the file thread, by the names they give
// themselves. No other thread is sampled while a loading screen is up.
// A leaf is time in a function of the game's; an inner row is time in the
// system or a library on the game's behalf, given to the game's function
// that called out; a stack row is time under a function, itself included.
#pragma once

#include <cstdint>

namespace probe
{

// What the loading mode is doing, as one word: 0 for no loading screen.
constexpr std::uint32_t LoadingPhase(std::uint16_t state, std::int32_t step, bool inside)
{
    constexpr std::uint32_t kInside = 0x80000000;
    return (inside ? kInside : 0) | (static_cast<std::uint32_t>(state & 0x7FFF) << 16) |
           (static_cast<std::uint32_t>(step) & 0xFFFF);
}

// The loading watch says what the loading mode is doing, from the thread
// that runs its update: a phase, or 0 when the screen is over.
void SetLoadingPhase(std::uint32_t phase);

// Starts the thread. False if it could not be started or the exe's code
// section could not be found; the reason is in probe.log.
bool StartRecorder();

} // namespace probe
