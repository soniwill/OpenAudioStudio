#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace oae::bankc {

/** Owns an OS destination lock from recovery through compilation and publication.
 * Return codes are 0 success, 1 ordinary failure, 2 busy, and 3 recovery required.
 * The optional phase hook is for deterministic fault-injection tests only.
 */
class PublicationSession {
public:
    using PhaseHook = std::function<void(const std::string&, const std::filesystem::path&)>;

    PublicationSession(std::filesystem::path destination, std::ostream& diagnostics,
                       PhaseHook hook = {});
    ~PublicationSession();
    PublicationSession(const PublicationSession&) = delete;
    PublicationSession& operator=(const PublicationSession&) = delete;

    /** Acquires the lock, recovers pending work and verifies the current public pair. */
    int open();

    /** Publishes a validated bank/catalog pair while retaining the destination lock. */
    int publish(const std::vector<std::uint8_t>& bank, const std::string& catalog);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
