/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Offline NP layer: the online services the NP toolkit (linked natively, ledger row G-6) and the title
 * import. The port runs offline: lifecycle calls (initialize, terminate, contexts, callback registration,
 * delete, abort, close) succeed; everything that would reach the network or create an online object
 * reports SCE_NP_ERROR_SIGNED_OUT, so the title's own offline paths take over. Every name is listed
 * exactly (no prefix or hash matching); the table was generated from the import report of the linked
 * image and the public NID database. */
#include "runtime.h"
#include <stdio.h>

#define NP_ERROR_SIGNED_OUT ((int32_t)0x80550006)

static unsigned long np_calls_ok, np_calls_offline;
static ABI int32_t np_ok(void) { ++np_calls_ok; return 0; }
static ABI int32_t np_offline(void) { ++np_calls_offline; return NP_ERROR_SIGNED_OUT; }

static const RuntimeExport exports[]={
    {"sceGameCustomDataDialogInitialize",np_ok}, {"sceGameCustomDataDialogTerminate",np_ok},
    {"sceGameCustomDataDialogUpdateStatus",np_ok}, {"sceNpAuthAbortRequest",np_ok},
    {"sceNpBandwidthTestShutdown",np_ok}, {"sceNpCheckCallbackForLib",np_ok},
    {"sceNpInGameMessageDeleteHandle",np_ok}, {"sceNpInGameMessageInitialize",np_ok},
    {"sceNpInGameMessageTerminate",np_ok}, {"sceNpMatching2RegisterRoomMessageCallback",np_ok},
    {"sceNpRegisterStateCallbackForToolkit",np_ok}, {"sceNpSnsFacebookDeleteRequest",np_ok},
    {"sceNpSnsFacebookDialogInitialize",np_ok}, {"sceNpSnsFacebookDialogTerminate",np_ok},
    {"sceNpSnsFacebookDialogUpdateStatus",np_ok}, {"sceNpTusDeleteMultiSlotData",np_ok},
    {"sceNpTusDeleteMultiSlotDataVUser",np_ok}, {"sceNpTusDeleteNpTitleCtx",np_ok},
    {"sceNpTusDeleteRequest",np_ok}, {"sceNpUnregisterStateCallbackForToolkit",np_ok},
    {"sceNpWebApiDeleteHandle",np_ok}, {"sceNpWebApiDeleteServicePushEventFilter",np_ok},
    {"sceNpWebApiRegisterServicePushEventCallback",np_ok},
    {"sceNpWebApiUnregisterServicePushEventCallback",np_ok}, {"sceNpWordFilterDeleteRequest",np_ok},
    {"sceNpWordFilterDeleteTitleCtx",np_ok}, {"sceSigninDialogClose",np_ok},
    {"sceSigninDialogInitialize",np_ok}, {"sceSigninDialogTerminate",np_ok},
    {"sceSigninDialogUpdateStatus",np_ok},
    {"sceGameCustomDataDialogGetResult",np_offline}, {"sceGameCustomDataDialogOpen",np_offline},
    {"sceInvitationDialogGetResult",np_offline}, {"sceNpAuthCreateRequest",np_offline},
    {"sceNpBandwidthTestGetStatus",np_offline}, {"sceNpBandwidthTestInitStart",np_offline},
    {"sceNpCommerceDialogGetResult",np_offline}, {"sceNpCommerceHidePsStoreIcon",np_offline},
    {"sceNpCreateRequest",np_offline}, {"sceNpInGameMessageCreateHandle",np_offline},
    {"sceNpInGameMessagePrepare",np_offline}, {"sceNpInGameMessageSendData",np_offline},
    {"sceNpLookupCreateRequest",np_offline}, {"sceNpMatching2GetRoomDataExternalList",np_offline},
    {"sceNpMatching2GetRoomDataInternal",np_offline}, {"sceNpMatching2GetRoomMemberDataInternal",np_offline},
    {"sceNpMatching2GetUserInfoList",np_offline}, {"sceNpMatching2SendRoomChatMessage",np_offline},
    {"sceNpMatching2SendRoomMessage",np_offline}, {"sceNpMatching2SetUserInfo",np_offline},
    {"sceNpMatching2SignalingGetConnectionInfo",np_offline}, {"sceNpScoreGetFriendsRanking",np_offline},
    {"sceNpScoreGetFriendsRankingAsync",np_offline}, {"sceNpScoreGetGameDataAsync",np_offline},
    {"sceNpScoreGetRankingByNpId",np_offline}, {"sceNpScoreGetRankingByNpIdAsync",np_offline},
    {"sceNpScoreGetRankingByRangeAsync",np_offline}, {"sceNpScorePollAsync",np_offline},
    {"sceNpScoreRecordGameDataAsync",np_offline}, {"sceNpScoreRecordScoreAsync",np_offline},
    {"sceNpSnsFacebookCreateRequest",np_offline}, {"sceNpSnsFacebookDialogGetResult",np_offline},
    {"sceNpSnsFacebookDialogOpen",np_offline}, {"sceNpSnsFacebookGetAccessToken",np_offline},
    {"sceNpTrophyCaptureScreenshot",np_offline}, {"sceNpTrophyGetGameIcon",np_offline},
    {"sceNpTrophyGetGroupIcon",np_offline}, {"sceNpTrophyGetGroupInfo",np_offline},
    {"sceNpTrophyGetTrophyIcon",np_offline}, {"sceNpTssGetData",np_offline},
    {"sceNpTusAddAndGetVariable",np_offline}, {"sceNpTusAddAndGetVariableVUser",np_offline},
    {"sceNpTusCreateNpTitleCtx",np_offline}, {"sceNpTusCreateRequest",np_offline},
    {"sceNpTusGetData",np_offline}, {"sceNpTusGetDataVUser",np_offline},
    {"sceNpTusGetMultiSlotVariable",np_offline}, {"sceNpTusGetMultiSlotVariableVUser",np_offline},
    {"sceNpTusSetData",np_offline}, {"sceNpTusSetDataVUser",np_offline},
    {"sceNpTusSetMultiSlotVariable",np_offline}, {"sceNpTusSetMultiSlotVariableVUser",np_offline},
    {"sceNpWebApiCreateHandle",np_offline}, {"sceNpWebApiCreateServicePushEventFilter",np_offline},
    {"sceNpWebApiSendRequest2",np_offline}, {"sceNpWordFilterCreateAsyncRequest",np_offline},
    {"sceNpWordFilterCreateRequest",np_offline}, {"sceNpWordFilterCreateTitleCtx",np_offline},
    {"sceNpWordFilterPollAsync",np_offline}, {"sceNpWordFilterSanitizeComment",np_offline},
    {"sceSigninDialogGetResult",np_offline}, {"sceSigninDialogOpen",np_offline},
};
uintptr_t runtime_np_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_np_report(void) { printf("Runtime: offline NP calls succeeded=%lu, signed-out=%lu\n",np_calls_ok,np_calls_offline); }
