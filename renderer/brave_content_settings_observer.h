/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
#define BRAVE_RENDERER_CONTENT_SETTINGS_OBSERVER_H_

#include "base/strings/string16.h"
#include "chrome/renderer/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace blink {
class WebLocalFrame;
}

// Handles blocking content per content settings for each RenderFrame.
class BraveContentSettingsObserver
    : public ContentSettingsObserver {
 public:
  BraveContentSettingsObserver(content::RenderFrame* render_frame,
                               extensions::Dispatcher* extension_dispatcher,
                               bool should_whitelist,
                               service_manager::BinderRegistry* registry);
  ~BraveContentSettingsObserver() override;

 protected:
  bool AllowScript(bool enabled_per_settings) override;
  bool AllowScriptFromSource(bool enabled_per_settings,
      const blink::WebURL& script_url) override;

  bool AllowFingerprinting(bool enabled_per_settings) override;

  void BraveSpecificDidBlockJavaScript(
    const base::string16& details);

  void DidBlockFingerprinting(
    const base::string16& details);

 private:
  GURL GetOriginOrURL(const blink::WebFrame* frame);

  ContentSetting GetContentSettingFromRules(
      const ContentSettingsForOneType& rules,
      const blink::WebFrame* frame,
      const GURL& secondary_url);

  bool IsBraveShieldsDown(
      const blink::WebFrame* frame,
      const GURL& secondary_url);

  // RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnAllowScriptsOnce(const std::vector<std::string>& origins);
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;

  bool IsScriptTemporilyAllowed(const GURL& script_url);

  // Origins of scripts which are temporary allowed for this frame in the
  // current load
  base::flat_set<std::string> temporarily_allowed_scripts_;

  // temporary allowed script origins we preloaded for the next load
  base::flat_set<std::string> preloaded_temporarily_allowed_scripts_;

  DISALLOW_COPY_AND_ASSIGN(BraveContentSettingsObserver);
};

#endif  // BRAVE_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
