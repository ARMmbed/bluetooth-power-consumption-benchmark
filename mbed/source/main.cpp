/* mbed Microcontroller Library
 * Copyright (c) 2006-2021 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cctype>
#include <chrono>
#include <inttypes.h>

#include <ble/BLE.h>
#include <mbed-trace/mbed_trace.h>

#include <MbedBluetoothPlatform.h>
#include <PowerConsumptionTest.h>

/** This program enters different BLE states according to operator input, allowing power consumption to be
 * measured.
 */

using ble::BLE;

events::EventQueue event_queue;

void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
{
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    mbed_trace_init();

    BLE &ble = BLE::Instance();
    MbedBluetoothPlatform platform(ble, event_queue);
    PowerConsumptionTest app(platform);

    ble.onEventsToProcess(schedule_ble_events);
    app.run();

    return 0;
}
