// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

/**
 * @brief HTML/CSS/JavaScript provider for the built-in browser UI.
 *
 * WebInterface is responsible for the human-facing page only. It decides
 * whether an incoming HTTP request should receive the embedded UI document and
 * generates the complete page markup used by the in-browser control surface.
 *
 * The generated page includes:
 * - runtime status and version presentation
 * - parameter inspection and editing for the configured source node
 * - graph display
 * - websocket-driven frame-context updates
 * - demand-driven raw frame transfer and browser-side rendering tools
 *
 * Socket creation, request parsing, websocket upgrades and error handling are
 * intentionally kept out of this class and remain in @ref WebServer so the UI
 * document can evolve independently from the transport layer.
 *
 * @see WebServer
 */
class WebInterface
{
public:
    /**
     * @brief Creates the UI handler for the given source node identifier.
     *
     * The source node id is injected into the generated page so browser-side
     * parameter requests target the active UI source (`v4l2src0` in UI mode).
     */
    explicit WebInterface(std::string sourceNodeId);

    /**
     * @brief Serves the integrated root page when the request targets `/`.
     *
     * Returns @c false for all other paths so the caller can delegate to other
     * request handlers.
     */
    bool tryHandle(const std::string& method, const std::string& path, std::string& responseBody, std::string& contentType, int& statusCode) const;

private:
    std::string renderPage() const;

    std::string m_sourceNodeId;
};