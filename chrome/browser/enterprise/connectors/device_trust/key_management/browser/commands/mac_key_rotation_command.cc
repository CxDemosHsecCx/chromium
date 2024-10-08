// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kStableChannelHostName[] = "m.google.com";

bool ValidRotationCommand(const std::string& host_name) {
  return chrome::GetChannel() != version_info::Channel::STABLE ||
         host_name == kStableChannelHostName;
}

// Allows the key rotation maanger to be released in the correct worker thread.
void OnBackgroundTearDown(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    base::OnceCallback<void(KeyRotationResult)> result_callback,
    KeyRotationResult result) {
  std::move(result_callback).Run(result);
}

// Runs on the thread pool.
void StartRotation(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const std::string& nonce,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    base::OnceCallback<void(KeyRotationResult)> result_callback) {
  // TODO(b/351201459): When DTCKeyRotationUploadedBySharedAPIEnabled is fully
  // launched, remove `dm_server_url` and `dm_token` from this function.

  CHECK(pending_url_loader_factory);

  std::unique_ptr<KeyRotationManager> key_rotation_manager = nullptr;
  if (IsDTCKeyRotationUploadedBySharedAPI()) {
    CHECK(dm_token_storage);
    CHECK(device_management_service);
    key_rotation_manager = KeyRotationManager::Create(
        std::make_unique<
            enterprise_attestation::BrowserCloudManagementDelegate>(
            dm_token_storage, enterprise_attestation::DMServerClient::Create(
                                  device_management_service,
                                  network::SharedURLLoaderFactory::Create(
                                      std::move(pending_url_loader_factory)))));

  } else {
    key_rotation_manager =
        KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
            network::SharedURLLoaderFactory::Create(
                std::move(pending_url_loader_factory))));
  }

  CHECK(key_rotation_manager);

  auto* key_rotation_manager_ptr = key_rotation_manager.get();
  key_rotation_manager_ptr->Rotate(
      dm_server_url, dm_token, nonce,
      base::BindOnce(&OnBackgroundTearDown, std::move(key_rotation_manager),
                     std::move(result_callback)));
}

}  // namespace

MacKeyRotationCommand::MacKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      client_(SecureEnclaveClient::Create()) {
  CHECK(url_loader_factory_);
  CHECK(client_);
}

MacKeyRotationCommand::MacKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service)
    : url_loader_factory_(std::move(url_loader_factory)),
      dm_token_storage_(dm_token_storage),
      device_management_service_(device_management_service),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      client_(SecureEnclaveClient::Create()) {
  CHECK(IsDTCKeyRotationUploadedBySharedAPI());
  CHECK(dm_token_storage_);
  CHECK(device_management_service_);
  CHECK(url_loader_factory_);
  CHECK(client_);
}

MacKeyRotationCommand::~MacKeyRotationCommand() = default;

void MacKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Parallel usage of command objects is not supported.
  CHECK(!pending_callback_);

  if (!client_->VerifySecureEnclaveSupported()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. The secure enclave is "
                     "not supported.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED_OS_RESTRICTION);
    return;
  }

  GURL dm_server_url(params.dm_server_url);
  // TODO(b/351201459): When IsDTCKeyRotationUploadedBySharedAPI is fully
  // launched, ignore `dm_server_url` and `dm_token`.
  if (!IsDTCKeyRotationUploadedBySharedAPI()) {
    if (!ValidRotationCommand(dm_server_url.host())) {
      SYSLOG(ERROR)
          << "Device trust key rotation failed. The server URL is invalid.";
      std::move(callback).Run(KeyRotationCommand::Status::FAILED);
      return;
    }
  }

  pending_callback_ = std::move(callback);

  timeout_timer_.Start(
      FROM_HERE, timeouts::kProcessWaitTimeout,
      base::BindOnce(&MacKeyRotationCommand::OnKeyRotationTimeout,
                     weak_factory_.GetWeakPtr()));

  auto rotation_result_callback =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &MacKeyRotationCommand::OnKeyRotated, weak_factory_.GetWeakPtr()));

  if (IsDTCKeyRotationUploadedBySharedAPI()) {
    // Kicks off the key rotation process in a worker thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StartRotation, dm_server_url, params.dm_token,
                       params.nonce, url_loader_factory_->Clone(),
                       dm_token_storage_, device_management_service_,
                       std::move(rotation_result_callback)));

    return;
  }

  // Kicks off the key rotation process in a worker thread.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StartRotation, dm_server_url, params.dm_token,
                                params.nonce, url_loader_factory_->Clone(),
                                dm_token_storage_, device_management_service_,
                                std::move(rotation_result_callback)));
}

void MacKeyRotationCommand::OnKeyRotated(KeyRotationResult result) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  if (!pending_callback_) {
    // The callback may have already run in timeout cases.
    return;
  }

  timeout_timer_.Stop();

  auto response_status = KeyRotationCommand::Status::FAILED;
  switch (result) {
    case KeyRotationResult::kSucceeded:
      response_status = KeyRotationCommand::Status::SUCCEEDED;
      break;
    case KeyRotationResult::kFailed:
      SYSLOG(ERROR) << "Device trust key rotation failed.";
      response_status = KeyRotationCommand::Status::FAILED;
      break;
    case KeyRotationResult::kInsufficientPermissions:
      SYSLOG(ERROR) << "Device trust key rotation failed. The browser is "
                       "missing permissions.";
      response_status = KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS;
      break;
    case KeyRotationResult::kFailedKeyConflict:
      SYSLOG(ERROR) << "Device trust key rotation failed. Confict with the key "
                       "that exists on the server.";
      response_status = KeyRotationCommand::Status::FAILED_KEY_CONFLICT;
      break;
  }

  std::move(pending_callback_).Run(response_status);
}

void MacKeyRotationCommand::OnKeyRotationTimeout() {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // A callback should still be available to be run.
  if (!pending_callback_) {
    // The callback may have already run in timeout cases.
    return;
  }

  SYSLOG(ERROR) << "Device trust key rotation timed out.";
  std::move(pending_callback_).Run(KeyRotationCommand::Status::TIMED_OUT);
}

}  // namespace enterprise_connectors
