/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/renderer/brave_content_settings_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "brave/common/render_messages.h"
#include "brave/content/common/frame_messages.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

BraveContentSettingsObserver::BraveContentSettingsObserver(
    content::RenderFrame* render_frame,
    extensions::Dispatcher* extension_dispatcher,
    bool should_whitelist,
    service_manager::BinderRegistry* registry)
    : ContentSettingsObserver(render_frame, extension_dispatcher,
          should_whitelist, registry) {
}

BraveContentSettingsObserver::~BraveContentSettingsObserver() {
}

void BraveContentSettingsObserver::DidFinishLoad() {
  temporarily_allowed_scripts_.clear();
}

bool BraveContentSettingsObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BraveContentSettingsObserver, message)
    IPC_MESSAGE_HANDLER(BraveFrameMsg_AllowScriptsOnce, OnAllowScriptsOnce)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return ContentSettingsObserver::OnMessageReceived(message);
}

void BraveContentSettingsObserver::OnAllowScriptsOnce(
    const std::vector<std::string>& origins) {
  temporarily_allowed_scripts_.insert(origins.begin(), origins.end());
}

bool BraveContentSettingsObserver::IsScriptTemporilyAllowed(
    const GURL& script_url) {
  // check if scripts from this origin are temporily allowed or not
  return base::ContainsKey(temporarily_allowed_scripts_,
      script_url.GetOrigin().spec());
}

void BraveContentSettingsObserver::BraveSpecificDidBlockJavaScript(
    const base::string16& details) {
  Send(new BraveViewHostMsg_JavaScriptBlocked(routing_id(), details));
}

bool BraveContentSettingsObserver::AllowScript(
    bool enabled_per_settings) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (frame && IsScriptTemporilyAllowed(
        url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL())) {
    return true;
  }

  return ContentSettingsObserver::AllowScript(enabled_per_settings);
}

bool BraveContentSettingsObserver::AllowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {

  if (IsScriptTemporilyAllowed(GURL(script_url))) return true;

  bool allow = ContentSettingsObserver::AllowScriptFromSource(
      enabled_per_settings, script_url);
  if (!allow) {
    const GURL secondary_url(script_url);
    BraveSpecificDidBlockJavaScript(base::UTF8ToUTF16(secondary_url.spec()));
  }
  return allow;
}

void BraveContentSettingsObserver::DidBlockFingerprinting(
    const base::string16& details) {
  Send(new BraveViewHostMsg_FingerprintingBlocked(routing_id(), details));
}

GURL BraveContentSettingsObserver::GetOriginOrURL(const blink::WebFrame* frame) {
  url::Origin top_origin = url::Origin(frame->Top()->GetSecurityOrigin());
  // The |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  // TODO(alexmos): This is broken for --site-per-process, since top() can be a
  // WebRemoteFrame which does not have a document(), and the WebRemoteFrame's
  // URL is not replicated.  See https://crbug.com/628759.
  if (top_origin.unique() && frame->Top()->IsWebLocalFrame())
    return frame->Top()->ToWebLocalFrame()->GetDocument().Url();
  return top_origin.GetURL();
}

ContentSetting BraveContentSettingsObserver::GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const blink::WebLocalFrame* frame,
    const GURL& secondary_url) {

  const GURL& primary_url = GetOriginOrURL(frame);

  for (const auto& rule : rules) {
    ContentSettingsPattern secondary_pattern = rule.secondary_pattern;
    if (rule.secondary_pattern ==
        ContentSettingsPattern::FromString("https://firstParty/*")) {
      secondary_pattern = ContentSettingsPattern::FromString(
          "[*.]" + GetOriginOrURL(frame).HostNoBrackets());
    }

    if (rule.primary_pattern.Matches(primary_url) &&
        (secondary_pattern == ContentSettingsPattern::Wildcard() ||
         secondary_pattern.Matches(secondary_url))) {
      return rule.GetContentSetting();
    }
  }

  // for cases which are third party resources and doesn't match any existing
  // rules, block them by default
  return CONTENT_SETTING_BLOCK;
}

bool BraveContentSettingsObserver::AllowFingerprinting(
    bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  bool allow = true;
  const GURL secondary_url(
      url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL());
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->fingerprinting_rules, frame, secondary_url);
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsWhitelistedForContentSettings();

  if (!allow) {
    DidBlockFingerprinting(base::UTF8ToUTF16(secondary_url.spec()));
  }

  return allow;
}
