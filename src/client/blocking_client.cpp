#include "blocking_client.h"
#include "blocking_client_impl.h"

namespace kvstore {

BlockingClient::BlockingClient(const Config& config, int32_t client_id)
    : impl_(std::make_unique<BlockingClientImpl>(config, client_id)) {
}

BlockingClient::~BlockingClient() {
}

bool BlockingClient::Read(const std::string& key, std::string& value) {
    return impl_->Read(key, value);
}

bool BlockingClient::Write(const std::string& key, const std::string& value) {
    return impl_->Write(key, value);
}

int64_t BlockingClient::GetCurrentTimestamp() const {
    return impl_->GetCurrentTimestamp();
}

}
