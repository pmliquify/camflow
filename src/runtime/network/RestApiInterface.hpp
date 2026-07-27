// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "core/RuntimeController.hpp"
#include "pipeline/NodeFactory.hpp"

#include <string>

/**
 * @brief Request router and JSON serializer for the built-in runtime REST API.
 *
 * RestApiInterface owns no sockets and runs no background work. Its single role
 * is to translate HTTP requests that already reached the service layer into
 * runtime operations and JSON responses.
 *
 * Implemented endpoint groups:
 * - runtime state and version metadata
 * - pipeline graph inspection
 * - pipeline graph replacement
 * - node-type enumeration
 * - live node schema and parameter inspection
 * - runtime parameter updates
 * - runtime state transitions
 *
 * The interface is intentionally stateless apart from references to the node
 * registry and the application-owned @ref RuntimeController. This keeps request
 * routing deterministic and makes the socket/service concerns in @ref WebServer
 * independent from the REST payload logic.
 *
 * @see WebServer
 * @see RuntimeController
 * @see NodeFactory
 */
class RestApiInterface
{
public:
    /**
     * @brief Binds the REST layer to the node registry and running controller.
     * @param factory Node type registry used for `/api/nodes` and schema fallback.
     * @param controller Application-owned runtime controller used for all live operations.
     * @param uiModeEnabled Indicates whether the built-in UI source-only mode is active.
     */
    RestApiInterface(const NodeFactory& factory, RuntimeController& controller, bool uiModeEnabled);

    /**
     * @brief Tries to handle one REST request and serializes the response.
     *
     * Returns @c true only when the request path belongs to the REST API and a
     * response payload was produced. Unknown paths are left to other interface
     * layers such as @ref WebInterface.
     */
    bool tryHandle(const std::string& method, const std::string& path, const std::string& body, std::string& responseBody, std::string& contentType, int& statusCode) const;

private:
    std::string runtimeStateJson() const;
    std::string runtimeVersionJson() const;

    const NodeFactory& m_factory;
    RuntimeController& m_controller;
    bool m_uiModeEnabled;
};