/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILocaleService.h"
#include "nsISpeechService.h"
#include "nsServiceManagerUtils.h"

#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"
#include "nsSynthVoiceRegistry.h"
#include "nsSpeechTask.h"

#include "nsString.h"
#include "mozilla/StaticPtr.h"

#undef LOG
#ifdef PR_LOGGING
extern PRLogModuleInfo* GetSpeechSynthLog();
#define LOG(type, msg) PR_LOG(GetSpeechSynthLog(), type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {
namespace dom {

// VoiceData

class VoiceData
{
public:
  VoiceData(nsISpeechService* aService, const nsAString& aUri,
            const nsAString& aName, const nsAString& aLang, bool aIsLocal)
    : mService(aService)
    , mUri(aUri)
    , mName(aName)
    , mLang(aLang)
    , mIsLocal(aIsLocal) {}

  ~VoiceData() {}

  NS_INLINE_DECL_REFCOUNTING(VoiceData)

  nsCOMPtr<nsISpeechService> mService;

  nsString mUri;

  nsString mName;

  nsString mLang;

  bool mIsLocal;
};

// nsSynthVoiceRegistry

static StaticRefPtr<nsSynthVoiceRegistry> gSynthVoiceRegistry;

NS_IMPL_ISUPPORTS1(nsSynthVoiceRegistry, nsISynthVoiceRegistry)

nsSynthVoiceRegistry::nsSynthVoiceRegistry()
{
  mUriVoiceMap.Init();
}

nsSynthVoiceRegistry::~nsSynthVoiceRegistry()
{
  LOG(PR_LOG_DEBUG, ("~nsSynthVoiceRegistry"));

  mUriVoiceMap.Clear();
}

nsSynthVoiceRegistry*
nsSynthVoiceRegistry::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSynthVoiceRegistry) {
    gSynthVoiceRegistry = new nsSynthVoiceRegistry();
  }

  return gSynthVoiceRegistry;
}

already_AddRefed<nsSynthVoiceRegistry>
nsSynthVoiceRegistry::GetInstanceForService()
{
  nsRefPtr<nsSynthVoiceRegistry> registry = GetInstance();

  return registry.forget();
}

void
nsSynthVoiceRegistry::Shutdown()
{
  LOG(PR_LOG_DEBUG, ("nsSynthVoiceRegistry::Shutdown()"));
  gSynthVoiceRegistry = nullptr;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::AddVoice(nsISpeechService* aService,
                               const nsAString& aUri,
                               const nsAString& aName,
                               const nsAString& aLang,
                               bool aLocalService)
{
  LOG(PR_LOG_DEBUG,
      ("nsSynthVoiceRegistry::AddVoice uri='%s' name='%s' lang='%s' local=%s",
       NS_ConvertUTF16toUTF8(aUri).get(), NS_ConvertUTF16toUTF8(aName).get(),
       NS_ConvertUTF16toUTF8(aLang).get(),
       aLocalService ? "true" : "false"));

  return AddVoiceImpl(aService, aUri, aName, aLang,
                      aLocalService);
}

NS_IMETHODIMP
nsSynthVoiceRegistry::RemoveVoice(nsISpeechService* aService,
                                  const nsAString& aUri)
{
  LOG(PR_LOG_DEBUG,
      ("nsSynthVoiceRegistry::RemoveVoice uri='%s'",
       NS_ConvertUTF16toUTF8(aUri).get()));

  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);

  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(aService == retval->mService, NS_ERROR_INVALID_ARG);

  mVoices.RemoveElement(retval);
  mDefaultVoices.RemoveElement(retval);
  mUriVoiceMap.Remove(aUri);

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::SetDefaultVoice(const nsAString& aUri,
                                      bool aIsDefault)
{
  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);

  mDefaultVoices.RemoveElement(retval);

  LOG(PR_LOG_DEBUG, ("nsSynthVoiceRegistry::SetDefaultVoice %s %s",
                     NS_ConvertUTF16toUTF8(aUri).get(),
                     aIsDefault ? "true" : "false"));

  if (aIsDefault) {
    mDefaultVoices.AppendElement(retval);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceCount(uint32_t* aRetval)
{
  *aRetval = mVoices.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoice(uint32_t aIndex, nsAString& aRetval)
{
  NS_ENSURE_TRUE(aIndex < mVoices.Length(), NS_ERROR_INVALID_ARG);

  aRetval = mVoices[aIndex]->mUri;

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::IsDefaultVoice(const nsAString& aUri, bool* aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);

  for (int32_t i = mDefaultVoices.Length(); i > 0; ) {
    VoiceData* defaultVoice = mDefaultVoices[--i];

    if (voice->mLang.Equals(defaultVoice->mLang)) {
      *aRetval = voice == defaultVoice;
      return NS_OK;
    }
  }

  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::IsLocalVoice(const nsAString& aUri, bool* aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);

  *aRetval = voice->mIsLocal;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceLang(const nsAString& aUri, nsAString& aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);

  aRetval = voice->mLang;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceName(const nsAString& aUri, nsAString& aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_TRUE(found, NS_ERROR_NOT_AVAILABLE);

  aRetval = voice->mName;
  return NS_OK;
}

nsresult
nsSynthVoiceRegistry::AddVoiceImpl(nsISpeechService* aService,
                                   const nsAString& aUri,
                                   const nsAString& aName,
                                   const nsAString& aLang,
                                   bool aLocalService)
{
  bool found = false;
  mUriVoiceMap.GetWeak(aUri, &found);
  NS_ENSURE_FALSE(found, NS_ERROR_INVALID_ARG);

  nsRefPtr<VoiceData> voice = new VoiceData(aService, aUri, aName, aLang,
                                            aLocalService);

  mVoices.AppendElement(voice);
  mUriVoiceMap.Put(aUri, voice);

  return NS_OK;
}

bool
nsSynthVoiceRegistry::FindVoiceByLang(const nsAString& aLang,
                                      VoiceData** aRetval)
{
  nsAString::const_iterator dashPos, start, end;
  aLang.BeginReading(start);
  aLang.EndReading(end);

  while (true) {
    nsAutoString langPrefix(Substring(start, end));

    for (int32_t i = mDefaultVoices.Length(); i > 0; ) {
      VoiceData* voice = mDefaultVoices[--i];

      if (StringBeginsWith(voice->mLang, langPrefix)) {
        *aRetval = voice;
        return true;
      }
    }

    for (int32_t i = mVoices.Length(); i > 0; ) {
      VoiceData* voice = mVoices[--i];

      if (StringBeginsWith(voice->mLang, langPrefix)) {
        *aRetval = voice;
        return true;
      }
    }

    dashPos = end;
    end = start;

    if (!RFindInReadable(NS_LITERAL_STRING("-"), end, dashPos)) {
      break;
    }
  }

  return false;
}

VoiceData*
nsSynthVoiceRegistry::FindBestMatch(const nsAString& aUri,
                                    const nsAString& aLang)
{
  if (mVoices.IsEmpty()) {
    return nullptr;
  }

  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);

  if (found) {
    LOG(PR_LOG_DEBUG, ("nsSynthVoiceRegistry::FindBestMatch - Matched URI"));
    return retval;
  }

  // Try finding a match for given voice.
  if (!aLang.IsVoid() && !aLang.IsEmpty()) {
    if (FindVoiceByLang(aLang, &retval)) {
      LOG(PR_LOG_DEBUG,
          ("nsSynthVoiceRegistry::FindBestMatch - Matched language (%s ~= %s)",
           NS_ConvertUTF16toUTF8(aLang).get(),
           NS_ConvertUTF16toUTF8(retval->mLang).get()));

      return retval;
    }
  }

  // Try UI language.
  nsresult rv;
  nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoString uiLang;
  rv = localeService->GetLocaleComponentForUserAgent(uiLang);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (FindVoiceByLang(uiLang, &retval)) {
    LOG(PR_LOG_DEBUG,
        ("nsSynthVoiceRegistry::FindBestMatch - Matched UI language (%s ~= %s)",
         NS_ConvertUTF16toUTF8(uiLang).get(),
         NS_ConvertUTF16toUTF8(retval->mLang).get()));

    return retval;
  }

  // Try en-US, the language of locale "C"
  if (FindVoiceByLang(NS_LITERAL_STRING("en-US"), &retval)) {
    LOG(PR_LOG_DEBUG,
        ("nsSynthVoiceRegistry::FindBestMatch - Matched C locale language (en-US ~= %s)",
         NS_ConvertUTF16toUTF8(retval->mLang).get()));

    return retval;
  }

  // The top default voice is better than nothing...
  if (!mDefaultVoices.IsEmpty()) {
    return mDefaultVoices.LastElement();
  }

  return nullptr;
}

already_AddRefed<nsSpeechTask>
nsSynthVoiceRegistry::SpeakUtterance(SpeechSynthesisUtterance& aUtterance,
                                     const nsAString& aDocLang)
{
  nsString lang = nsString(aUtterance.mLang.IsEmpty() ? aDocLang : aUtterance.mLang);
  nsAutoString uri;

  if (aUtterance.mVoice) {
    aUtterance.mVoice->GetVoiceURI(uri);
  }

  nsSpeechTask* task  = new nsSpeechTask(&aUtterance);
  Speak(aUtterance.mText, lang, uri,
        aUtterance.Rate(), aUtterance.Pitch(), task);

  NS_IF_ADDREF(task);
  return task;
}

void
nsSynthVoiceRegistry::Speak(const nsAString& aText,
                            const nsAString& aLang,
                            const nsAString& aUri,
                            const float& aRate,
                            const float& aPitch,
                            nsSpeechTask* aTask)
{
  LOG(PR_LOG_DEBUG,
      ("nsSynthVoiceRegistry::Speak text='%s' lang='%s' uri='%s' rate=%f pitch=%f",
       NS_ConvertUTF16toUTF8(aText).get(), NS_ConvertUTF16toUTF8(aLang).get(),
       NS_ConvertUTF16toUTF8(aUri).get(), aRate, aPitch));

  VoiceData* voice = FindBestMatch(aUri, aLang);

  if (!voice) {
    NS_WARNING("No voices found.");
    aTask->DispatchError(0, 0);
    return;
  }

  LOG(PR_LOG_DEBUG, ("nsSynthVoiceRegistry::Speak - Using voice URI: %s",
                     NS_ConvertUTF16toUTF8(voice->mUri).get()));

  SpeechServiceType serviceType;

  DebugOnly<nsresult> rv = voice->mService->GetServiceType(&serviceType);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to get speech service type");

  if (serviceType == nsISpeechService::SERVICETYPE_INDIRECT_AUDIO) {
    aTask->SetIndirectAudio(true);
  }

  voice->mService->Speak(aText, voice->mUri, aRate, aPitch, aTask);
}

} // namespace dom
} // namespace mozilla
