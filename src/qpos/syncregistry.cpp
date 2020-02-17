// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

void SyncRegistry(QPRegistry *pregistry)
{
    while (!fShutdown)
    {
        {
            if (pregistry->IsInReplayMode())
            {
                pregistry->CheckSynced();
            }
        }
        MilliSleep(SYNCREG_SLEEP_MS);
    }
}
