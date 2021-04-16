/*
 *   BSD LICENSE
 *   Copyright (c) 2021 Samsung Electronics Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/cli/request_handler.h"

#include <iostream>

#include "src/cli/add_device_command.h"
#include "src/cli/apply_log_filter_command.h"
#include "src/cli/array_info_command.h"
#include "src/cli/cli_event_code.h"
#include "src/cli/cond_signal_command.h"
#include "src/cli/create_array_command.h"
#include "src/cli/create_device_command.h"
#include "src/cli/create_volume_command.h"
#include "src/cli/delete_array_command.h"
#include "src/cli/delete_volume_command.h"
#include "src/cli/device_monitoring_state_command.h"
#include "src/cli/exit_ibofos_command.h"
#include "src/cli/get_host_nqn_command.h"
#include "src/cli/get_ibofos_info_command.h"
#include "src/cli/get_max_volume_count_command.h"
#include "src/cli/get_version_command.h"
#include "src/cli/handle_wbt_command.h"
#include "src/cli/list_array_command.h"
#include "src/cli/list_array_device_command.h"
#include "src/cli/list_device_command.h"
#include "src/cli/list_volume_command.h"
#include "src/cli/list_wbt_command.h"
#include "src/cli/logger_info_command.h"
#include "src/cli/mount_array_command.h"
#include "src/cli/mount_ibofos_command.h"
#include "src/cli/mount_volume_command.h"
#include "src/cli/rebuild_perf_impact_command.h"
#include "src/cli/remove_device_command.h"
#include "src/cli/rename_volume_command.h"
#include "src/cli/reset_mbr_command.h"
#include "src/cli/resize_volume_command.h"
#include "src/cli/scan_device_command.h"
#include "src/cli/set_log_level_command.h"
#include "src/cli/smart_command.h"
#include "src/cli/start_device_monitoring_command.h"
#include "src/cli/stop_device_monitoring_command.h"
#include "src/cli/stop_rebuilding_command.h"
#include "src/cli/unmount_array_command.h"
#include "src/cli/unmount_ibofos_command.h"
#include "src/cli/unmount_volume_command.h"
#include "src/cli/update_volume_qos_command.h"
#include "src/cli/rename_volume_command.h"
#include "src/cli/resize_volume_command.h"
#include "src/cli/get_max_volume_count_command.h"
#include "src/cli/get_host_nqn_command.h"
#include "src/cli/cond_signal_command.h"
#include "src/cli/mount_ibofos_command.h"
#include "src/cli/unmount_ibofos_command.h"
#include "src/cli/get_ibofos_info_command.h"
#include "src/cli/exit_ibofos_command.h"
#include "src/cli/stop_rebuilding_command.h"
#include "src/cli/rebuild_perf_impact_command.h"
#include "src/cli/apply_log_filter_command.h"
#include "src/cli/set_log_level_command.h"
#include "src/cli/handle_wbt_command.h"
#include "src/cli/list_wbt_command.h"
#include "src/cli/update_event_wrr_policy_command.h"
#include "src/cli/reset_event_wrr_policy_command.h"
#include "src/cli/update_volume_min_policy_command.h"

namespace pos_cli
{
bool RequestHandler::isExit = false;
RequestHandler::RequestHandler(void)
{
    cmdDictionary["GETVERSION"] = new GetVersionCommand();
    cmdDictionary["SMART"] = new SmartCommand();
    cmdDictionary["LOGGERINFO"] = new LoggerInfoCommand();
    cmdDictionary["SCANDEVICE"] = new ScanDeviceCommand();
    cmdDictionary["LISTDEVICE"] = new ListDeviceCommand();
    cmdDictionary["ADDDEVICE"] = new AddDeviceCommand();
    cmdDictionary["REMOVEDEVICE"] = new RemoveDeviceCommand();
    cmdDictionary["CREATEDEVICE"] = new CreateDeviceCommand();
    cmdDictionary["STARTDEVICEMONITORING"] = new StartDeviceMonitoringCommand();
    cmdDictionary["STOPDEVICEMONITORING"] = new StopDeviceMonitoringCommand();
    cmdDictionary["DEVICEMONITORINGSTATE"] = new DeviceMonitoringStateCommand();
    cmdDictionary["LISTARRAY"] = new ListArrayCommand();
    cmdDictionary["CREATEARRAY"] = new CreateArrayCommand();
    cmdDictionary["DELETEARRAY"] = new DeleteArrayCommand();
    cmdDictionary["MOUNTARRAY"] = new MountArrayCommand();
    cmdDictionary["UNMOUNTARRAY"] = new UnmountArrayCommand();
    cmdDictionary["LISTARRAYDEVICE"] = new ListArrayDeviceCommand();
    cmdDictionary["ARRAYINFO"] = new ArrayInfoCommand();
    cmdDictionary["RESETMBR"] = new ResetMbrCommand();
    cmdDictionary["CREATEVOLUME"] = new CreateVolumeCommand();
    cmdDictionary["DELETEVOLUME"] = new DeleteVolumeCommand();
    cmdDictionary["MOUNTVOLUME"] = new MountVolumeCommand();
    cmdDictionary["UNMOUNTVOLUME"] = new UnmountVolumeCommand();
    cmdDictionary["LISTVOLUME"] = new ListVolumeCommand();
    cmdDictionary["UPDATEVOLUMEQOS"] = new UpdateVolumeQosCommand();
    cmdDictionary["RENAMEVOLUME"] = new RenameVolumeCommand();
    cmdDictionary["RESIZEVOLUME"] = new ResizeVolumeCommand();
    cmdDictionary["GETMAXVOLUMECOUNT"] = new GetMaxVolumeCountCommand();
    cmdDictionary["GETHOSTNQN"] = new GetHostNqnCommand();
    cmdDictionary["CONDSIGNAL"] = new CondSignalCommand();
    cmdDictionary["MOUNTIBOFOS"] = new MountIbofosCommand();
    cmdDictionary["UNMOUNTIBOFOS"] = new UnmountIbofosCommand();
    cmdDictionary["GETIBOFOSINFO"] = new GetPosInfoCommand();
    cmdDictionary["EXITIBOFOS"] = new ExitIbofosCommand();
    cmdDictionary["STOPREBUILDING"] = new StopRebuildingCommand();
    cmdDictionary["REBUILDPERFIMPACT"] = new RebuildPerfImpactCommand();
    cmdDictionary["APPLYLOGFILTER"] = new ApplyLogFilterCommand();
    cmdDictionary["SETLOGLEVEL"] = new SetLogLevelCommand();
    cmdDictionary["LISTWBT"] = new ListWbtCommand();
    cmdDictionary["WBT"] = new HandleWbtCommand();
    cmdDictionary["UPDATEEVENTWRRPOLICY"] = new UpdateEventWrrPolicyCommand();
    cmdDictionary["RESETEVENTWRRPOLICY"] = new ResetEventWrrPolicyCommand();
    cmdDictionary["UPDATEVOLUMEMINPOLICY"] = new UpdateVolumeMinPolicyCommand();
}

RequestHandler::~RequestHandler(void)
{
    for (auto& command : cmdDictionary)
    {
        delete command.second;
    }
}

string
RequestHandler::ProcessCommand(char* msg)
{
    try
    {
        json jsonDoc = json::parse(msg);
        cout << msg << endl;

        if (jsonDoc.contains("command") && jsonDoc.contains("rid"))
        {
            string command = jsonDoc["command"].get<std::string>();
            string rid = jsonDoc["rid"].get<std::string>();

            auto iter = cmdDictionary.find(command);
            if (iter != cmdDictionary.end())
            {
                return iter->second->Execute(jsonDoc, rid);
            }
            else
            {
                JsonFormat jFormat;
                return jFormat.MakeResponse(command, rid, BADREQUEST,
                    "wrong command", GetPosInfo());
            }
        }
        else
        {
            JsonFormat jFormat;
            return jFormat.MakeResponse("ERROR_RESPONSE", "UNKNOWN", BADREQUEST,
                "wrong parameter", GetPosInfo());
        }
    }
    catch (const std::exception& e)
    {
        JsonFormat jFormat;
        return jFormat.MakeResponse("EXCEPTION", "UNKNOWN", BADREQUEST,
            e.what(), GetPosInfo());
    }
}

string
RequestHandler::TimedOut(char* msg)
{
    json jsonDoc = json::parse(msg);
    string command = jsonDoc["command"].get<std::string>();
    string rid = jsonDoc["rid"].get<std::string>();

    JsonFormat jFormat;

    return jFormat.MakeResponse(command, rid, (int)POS_EVENT_ID::TIMED_OUT, "TIMED OUT",
        GetPosInfo());
}

string
RequestHandler::PosBusy(char* msg)
{
    json jsonDoc = json::parse(msg);
    string command = jsonDoc["command"].get<std::string>();
    string rid = jsonDoc["rid"].get<std::string>();

    JsonFormat jFormat;

    return jFormat.MakeResponse(command, rid, (int)POS_EVENT_ID::POS_BUSY, "POS IS BUSY",
        GetPosInfo());
}

bool
RequestHandler::IsExit(void)
{
    return isExit;
}

void
RequestHandler::SetExit(bool inputExit)
{
    isExit = inputExit;
}

}; // namespace pos_cli