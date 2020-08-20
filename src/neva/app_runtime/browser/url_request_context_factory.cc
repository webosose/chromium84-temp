// Copyright 2016-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "neva/app_runtime/browser/url_request_context_factory.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/os_crypt/os_crypt.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "neva/app_runtime/browser/app_runtime_browser_switches.h"
#include "neva/app_runtime/browser/app_runtime_http_user_agent_settings.h"
#include "services/network/public/cpp/network_switches.h"

using content::BrowserThread;

namespace neva_app_runtime {
namespace {

const char kCacheStoreFile[] = "Cache";
const char kCookieStoreFile[] = "Cookies";
const int kDefaultDiskCacheSize = 16 * 1024 * 1024;  // default size is 16MB

bool IgnoreCertificateErrors() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(switches::kIgnoreCertificateErrors);
}

}  // namespace

// Private classes to expose URLRequestContextGetter that call back to the
// URLRequestContextFactory to create the URLRequestContext on demand.
//
// The URLRequestContextFactory::URLRequestContextGetter class is used for both
// the system and media URLRequestCotnexts.
class URLRequestContextFactory::URLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(URLRequestContextFactory* factory, bool is_media)
      : is_media_(is_media), factory_(factory) {}

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      if (is_media_) {
        request_context_.reset(factory_->CreateMediaRequestContext());
      } else {
        request_context_.reset(factory_->CreateSystemRequestContext());
        // Set request context used by NSS for Crl requests.
        // net::SetURLRequestContextForNSSHttpIO(request_context_.get());
      }
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
  }

 private:
  ~URLRequestContextGetter() override {}

  const bool is_media_;
  URLRequestContextFactory* const factory_;
  std::unique_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

// The URLRequestContextFactory::MainURLRequestContextGetter class is used for
// the main URLRequestContext.
class URLRequestContextFactory::MainURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  MainURLRequestContextGetter(
      URLRequestContextFactory* factory,
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      : factory_(factory),
        cookie_path_(browser_context->GetPath().Append(kCookieStoreFile)),
        request_interceptors_(std::move(request_interceptors)),
        browser_context_(browser_context) {
    std::swap(protocol_handlers_, *protocol_handlers);
  }

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      request_context_.reset(factory_->CreateMainRequestContext(
          browser_context_, cookie_path_, &protocol_handlers_,
          std::move(request_interceptors_)));
      protocol_handlers_.clear();
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
  }

 private:
  ~MainURLRequestContextGetter() override {
  }

  URLRequestContextFactory* const factory_;
  base::FilePath cookie_path_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;
  content::BrowserContext* const browser_context_;
  std::unique_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(MainURLRequestContextGetter);
};

URLRequestContextFactory::URLRequestContextFactory()
    : app_network_delegate_(std::make_unique<net::NetworkDelegateImpl>()),
      system_network_delegate_(std::make_unique<net::NetworkDelegateImpl>()),
      system_dependencies_initialized_(false),
      main_dependencies_initialized_(false),
      media_dependencies_initialized_(false) {}

URLRequestContextFactory::~URLRequestContextFactory() {
}

void URLRequestContextFactory::InitializeOnUIThread(net::NetLog* net_log) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Cast http user agent settings must be initialized in UI thread
  // because it registers itself to pref notification observer which is not
  // thread safe.
  http_user_agent_settings_.reset(new AppRuntimeHttpUserAgentSettings());
  net_log_ = net_log;
}

net::URLRequestContextGetter* URLRequestContextFactory::CreateMainGetter(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  if (!main_getter_) {
    main_getter_ =
        new MainURLRequestContextGetter(this, browser_context, protocol_handlers,
                                        std::move(request_interceptors));
  }
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMainGetter() {
  CHECK(main_getter_.get());
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetSystemGetter() {
  if (!system_getter_.get()) {
    system_getter_ = new URLRequestContextGetter(this, false);
  }
  return system_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMediaGetter() {
  if (!media_getter_.get()) {
    media_getter_ = new URLRequestContextGetter(this, true);
  }
  return media_getter_.get();
}

void URLRequestContextFactory::InitializeSystemContextDependencies() {
  if (system_dependencies_initialized_)
    return;

  host_resolver_manager_ = std::make_unique<net::HostResolverManager>(
      net::HostResolver::ManagerOptions(),
      net::NetworkChangeNotifier::GetSystemDnsConfigNotifier(),
      /*net_log=*/nullptr);
  cert_verifier_ =
      net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr);
  ssl_config_service_.reset(new net::SSLConfigServiceDefaults);
  transport_security_state_.reset(new net::TransportSecurityState());
  cert_transparency_verifier_.reset(new net::MultiLogCTVerifier());
  ct_policy_enforcer_.reset(new net::DefaultCTPolicyEnforcer());

  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault();

  // Use in-memory HttpServerProperties. Disk-based can improve performance
  // but benefit seems small (only helps 1st request to a server).
  http_server_properties_.reset(new net::HttpServerProperties);

  std::unique_ptr<net::ProxyConfigService> proxy_config_service;
  net::ProxyConfig proxy_config;

  proxy_config_service.reset(new net::ProxyConfigServiceFixed(
      net::ProxyConfigWithAnnotation::CreateDirect()));

  proxy_resolution_service_ =
      net::ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service), nullptr, true);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string host_resolver_rules =
      cmd_line->GetSwitchValueASCII(network::switches::kHostResolverRules);
  if (host_resolver_rules.empty()) {
    system_host_resolver_ =
        net::HostResolver::CreateResolver(host_resolver_manager_.get());
  } else {
    std::unique_ptr<net::HostResolver> default_resolver(
        net::HostResolver::CreateResolver(host_resolver_manager_.get()));
    std::unique_ptr<net::MappedHostResolver> remapped_host_resolver(
        new net::MappedHostResolver(std::move(default_resolver)));
    remapped_host_resolver->SetRulesFromString(host_resolver_rules);
    system_host_resolver_ = std::move(remapped_host_resolver);
  }

  system_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMainContextDependencies(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  if (main_dependencies_initialized_)
    return;

  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  // Keep ProtocolHandlers added in sync with
  // CastContentBrowserClient::IsHandledURL().
  for (content::ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end(); ++it) {
    bool set_protocol =
        job_factory->SetProtocolHandler(it->first, std::move(it->second));
    DCHECK(set_protocol);
  }

  main_job_factory_ = std::move(job_factory);

  main_host_resolver_ =
      net::HostResolver::CreateResolver(host_resolver_manager_.get());

  main_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMediaContextDependencies() {
  if (media_dependencies_initialized_)
    return;

  media_host_resolver_ =
      net::HostResolver::CreateResolver(host_resolver_manager_.get());
  media_dependencies_initialized_ = true;
}

void URLRequestContextFactory::PopulateNetworkSessionParams(
    bool ignore_certificate_errors,
    net::HttpNetworkSession::Params* session_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  session_params->ignore_certificate_errors = ignore_certificate_errors;

  // Enable QUIC if instructed by DCS. This remains constant for the lifetime of
  // the process.
  //  session_params->enable_quic = chromecast::IsFeatureEnabled(kEnableQuic);
  //  LOG(INFO) << "Set HttpNetworkSessionParams.enable_quic = "
  //            << session_params->enable_quic;
  //
  //  // Disable idle sockets close on memory pressure, if instructed by DCS. On
  //  // memory constrained devices:
  //  // 1. if idle sockets are closed when memory pressure happens, cast_shell
  //  will
  //  // close and re-open lots of connections to server.
  //  // 2. if idle sockets are kept alive when memory pressure happens, this
  //  may
  //  // cause JS engine gc frequently, leading to JS suspending.
  //  session_params->disable_idle_sockets_close_on_memory_pressure =
  //      chromecast::IsFeatureEnabled(kDisableIdleSocketsCloseOnMemoryPressure);
  //  LOG(INFO) << "Set HttpNetworkSessionParams."
  //            << "disable_idle_sockets_close_on_memory_pressure = "
  //            <<
  //            session_params->disable_idle_sockets_close_on_memory_pressure;
}

std::unique_ptr<net::HttpNetworkSession>
URLRequestContextFactory::CreateNetworkSession(
    const net::URLRequestContext* context) {
  net::HttpNetworkSession::Params session_params;
  net::HttpNetworkSession::Context session_context;
  PopulateNetworkSessionParams(IgnoreCertificateErrors(), &session_params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &session_context);
  return std::make_unique<net::HttpNetworkSession>(session_params,
                                                   session_context);
}

net::URLRequestContext* URLRequestContextFactory::CreateSystemRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();
  system_job_factory_.reset(new net::URLRequestJobFactoryImpl());
  system_cookie_store_ =
      content::CreateCookieStore(content::CookieStoreConfig(), net_log_);

  net::URLRequestContext* system_context = new net::URLRequestContext();
  ConfigureURLRequestContext(system_context, system_job_factory_,
                             system_cookie_store_, system_network_delegate_,
                             system_host_resolver_);

  system_network_session_ = CreateNetworkSession(system_context);
  system_transaction_factory_ =
      std::make_unique<net::HttpNetworkLayer>(system_network_session_.get());
  system_context->set_http_transaction_factory(
      system_transaction_factory_.get());

  return system_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMediaRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(main_getter_.get())
      << "Getting MediaRequestContext before MainRequestContext";

  InitializeMediaContextDependencies();

  // Reuse main context dependencies except HostResolver and
  // HttpTransactionFactory.
  net::URLRequestContext* media_context = new net::URLRequestContext();
  ConfigureURLRequestContext(media_context, main_job_factory_,
                             main_cookie_store_, app_network_delegate_,
                             media_host_resolver_);

  media_network_session_ = CreateNetworkSession(media_context);
  media_transaction_factory_ =
      std::make_unique<net::HttpNetworkLayer>(media_network_session_.get());
  media_context->set_http_transaction_factory(media_transaction_factory_.get());

  return media_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMainRequestContext(
    content::BrowserContext* browser_context,
    const base::FilePath& cookie_path,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();
  InitializeMainContextDependencies(
      protocol_handlers, std::move(request_interceptors));

  content::CookieStoreConfig cookie_config(cookie_path, false, true);
  main_cookie_store_ = content::CreateCookieStore(cookie_config, net_log_);

  net::URLRequestContext* main_context = new net::URLRequestContext();
  ConfigureURLRequestContext(main_context, main_job_factory_,
                             main_cookie_store_, app_network_delegate_,
                             main_host_resolver_);

  main_network_session_ = CreateNetworkSession(main_context);
  int disk_cache_size = kDefaultDiskCacheSize;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(kDiskCacheSize))
    base::StringToInt(cmd_line->GetSwitchValueASCII(kDiskCacheSize),
                      &disk_cache_size);

  std::unique_ptr<net::HttpCache::DefaultBackend> main_backend =
      std::make_unique<net::HttpCache::DefaultBackend>(
          net::DISK_CACHE, net::CACHE_BACKEND_SIMPLE,
          browser_context->GetPath().Append(kCacheStoreFile), disk_cache_size,
          true);

  main_transaction_factory_ = std::make_unique<net::HttpCache>(
      main_network_session_.get(), std::move(main_backend), false);
  main_context->set_http_transaction_factory(main_transaction_factory_.get());

  return main_context;
}

void URLRequestContextFactory::ConfigureURLRequestContext(
    net::URLRequestContext* context,
    const std::unique_ptr<net::URLRequestJobFactory>& job_factory,
    const std::unique_ptr<net::CookieStore>& cookie_store,
    const std::unique_ptr<net::NetworkDelegate>& network_delegate,
    const std::unique_ptr<net::HostResolver>& host_resolver) {
  // common settings
  context->set_cert_verifier(cert_verifier_.get());
  context->set_cert_transparency_verifier(cert_transparency_verifier_.get());
  context->set_ct_policy_enforcer(ct_policy_enforcer_.get());
  context->set_proxy_resolution_service(proxy_resolution_service_.get());
  context->set_ssl_config_service(ssl_config_service_.get());
  context->set_transport_security_state(transport_security_state_.get());
  context->set_http_auth_handler_factory(http_auth_handler_factory_.get());
  context->set_http_server_properties(http_server_properties_.get());
  context->set_http_user_agent_settings(http_user_agent_settings_.get());
  context->set_net_log(net_log_);

  // settings from the caller
  context->set_job_factory(job_factory.get());
  context->set_cookie_store(cookie_store.get());
  context->set_network_delegate(network_delegate.get());
  context->set_host_resolver(host_resolver.get());

  host_resolver->SetRequestContext(context);
}

void URLRequestContextFactory::InitializeNetworkDelegates() {
  LOG(INFO) << "Initialized system network delegate.";
}

}  // namespace neva_app_runtime
