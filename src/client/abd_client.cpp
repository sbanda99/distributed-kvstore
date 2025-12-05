#include "abd_client.h"
#include "abd_client_impl.h"

namespace kvstore {

ABDClient::ABDClient(const Config& config) 
    : impl_(std::make_unique<ABDClientImpl>(config)) {
}

ABDClient::~ABDClient() {
}

bool ABDClient::Read(const std::string& key, std::string& value) {
    return impl_->Read(key, value);
}

bool ABDClient::Write(const std::string& key, const std::string& value) {
    return impl_->Write(key, value);
}

int64_t ABDClient::GetCurrentTimestamp() const {
    return impl_->GetCurrentTimestamp();
}

}
