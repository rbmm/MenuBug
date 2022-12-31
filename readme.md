probably bug in win32kfull!SfnINLPUAHNCPAINTMENUPOPUP
it do callback to user mode ( KeUserModeCallback ), while holding some global resourse (?)
if stop application during this callback:
stop work ALT+TAB, start menu, task bar, menu repaint (in taskmngr (after second try))
need logoff ( kill user session ) for restore after exe run


bool IS_USERCRIT_OWNED_AT_ALL()
{
  return ExIsResourceAcquiredSharedLite(SGDGetUserSessionState()->_pResource) != 0;
}

struct LeaveEnterCritProperDisposition {

  bool _b;

  LeaveEnterCritProperDisposition()
  {
    _b = ExIsResourceAcquiredExclusiveLite(SGDGetUserSessionState()->_pResource) && IS_USERCRIT_OWNED_AT_ALL();
    UserSessionSwitchLeaveCrit();
  }

  ~LeaveEnterCritProperDisposition
  {
    if (_b) EnterSharedCrit();
    EnterCrit(0, 0);
  }
};


in SfnINLPUAHNCPAINTMENUPOPUP

{
  LeaveEnterCritProperDisposition lock;
  EtwTraceBeginCallback(..);
  KeUserModeCallback(0x6E,...); // --> if stop app here, will be global effect 
  EtwTraceEndCallback(...);
}

ThreadUnlock1();