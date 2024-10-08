// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_CALL_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_CALL_H_

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/client_callback.h>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromecast/cast_core/grpc/grpc_call.h"
#include "chromecast/cast_core/grpc/grpc_client_reactor.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"

namespace cast {
namespace utils {

// Typedef for the server streaming method generated by gRPC compiler.
template <typename TAsyncInterface, typename TRequest, typename TResponse>
using GrpcServerStreamingMethod =
    void (TAsyncInterface::*)(grpc::ClientContext*,
                              const TRequest*,
                              grpc::ClientReadReactor<TResponse>*);

// A GrpcCall implementation for unary gRPC calls specialized by the
// |AsyncMethodPtr| function pointer.
//  TGrpcStub - gRPC service stub type.
//  TRequest - gRPC request type for a method in the stub.
//  TResponse - gRPC response type for a method in the stub.
//  AsyncMethodPtr - pointer to a method in the stub that handles a streaming
//  call.
template <typename TGrpcStub,
          typename TRequest,
          typename TResponse,
          GrpcServerStreamingMethod<typename TGrpcStub::AsyncInterface,
                                    TRequest,
                                    TResponse> AsyncMethodPtr>
class GrpcServerStreamingCall : public GrpcCall<TGrpcStub, TRequest> {
 public:
  using Base = GrpcCall<TGrpcStub, TRequest>;
  using Base::async;
  using Base::GrpcCall;
  using Base::request;
  using Base::sync;
  using typename Base::AsyncInterface;
  using typename Base::Context;
  using typename Base::Request;

  using Response = TResponse;
  using ResponseCallback =
      base::RepeatingCallback<void(GrpcStatusOr<Response>, bool /*done*/)>;

  // Invokes a gRPC call asynchronously. The method follows moves semantics:
  //   std::move(call).InvokeAsync(...);
  // The returned Context is valid only during duration of the call and can be
  // used to cancel it.
  Context InvokeAsync(ResponseCallback response_callback) && {
    // gRPC doesn't support setting a deadline for individual streaming
    // requests\responses. Hence, the zero timeout is set to allow for
    // inifinitely long streaming connections.
    Base::SetDeadline(0);
    auto reactor =
        new Reactor(std::move(*this).async(), std::move(*this).request(),
                    std::move(*this).options(), std::move(response_callback));
    reactor->Start();
    return Context(reactor->context());
  }

 private:
  using ReactorBase =
      GrpcClientReactor<Request, grpc::ClientReadReactor<Response>>;

  class Reactor final : public ReactorBase {
   public:
    using ReactorBase::context;
    using ReactorBase::request;

    Reactor(AsyncInterface* async_stub,
            Request request,
            GrpcCallOptions options,
            ResponseCallback response_callback)
        : ReactorBase(std::move(request), std::move(options)),
          async_interface_(async_stub),
          response_callback_(std::move(response_callback)) {}

    void Start() override {
      ReactorBase::Start();
      (async_interface_->*AsyncMethodPtr)(context(), request(), this);
      grpc::ClientReadReactor<Response>::StartRead(&response_);
      grpc::ClientReadReactor<Response>::StartCall();
    }

   private:
    // Implements grpc::ClientReadReactor APIs.
    void OnReadDone(bool ok) override {
      DVLOG(1) << "Reads done: ok=" << ok;
      if (!ok) {
        return;
      }

      response_callback_.Run(std::move(response_), false);
      Reactor::StartRead(&response_);
    }

    // The method is always called on completion of all operations associated
    // with this call, and deletes itself on exit.
    void OnDone(const grpc::Status& status) override {
      DVLOG(1) << "Request done: " << GrpcStatusToString(status);
      if (status.ok()) {
        response_callback_.Run(Response(), true);
      } else {
        response_callback_.Run(status, true);
      }
      ReactorBase::DeleteThis();
    }

    using AsyncStubCall =
        base::OnceCallback<void(grpc::ClientContext*,
                                const Request*,
                                grpc::ClientReadReactor<Response>*)>;

    raw_ptr<AsyncInterface> async_interface_;
    ResponseCallback response_callback_;
    Response response_;
  };
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_CALL_H_
