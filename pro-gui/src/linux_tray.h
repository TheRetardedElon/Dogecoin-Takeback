#pragma once
#if !defined(_WIN32)

void LinuxTrayInit();
void LinuxTrayWritePid();
void LinuxTrayClearPid();
void LinuxTrayEnsureHelper(const char* installDir);
void LinuxTrayNotify(const char* title, const char* body);
bool LinuxTrayPollShow();
/** SIGUSR2 from a tray helper: File→Exit equivalent (stop node). */
bool LinuxTrayPollQuit();

#endif
