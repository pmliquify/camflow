// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "WebInterface.hpp"

#include "web/WebUiAssets.hpp"

#include <utility>

namespace
{

std::string requestPathWithoutQuery(const std::string& path)
{
    return path.substr(0, path.find("?"));
}

void replaceAll(std::string& text, const std::string& pattern, const std::string& replacement)
{
    if (pattern.empty()) {
        return;
    }
    size_t cursor = 0;
    while (true) {
        const size_t pos = text.find(pattern, cursor);
        if (pos == std::string::npos) {
            break;
        }
        text.replace(pos, pattern.size(), replacement);
        cursor = pos + replacement.size();
    }
}

std::string injectedBootstrapMarkup(const std::string& sourceNodeId)
{
    return std::string() + "<script>window.CAMFLOW_SOURCE_NODE_ID = \"" + sourceNodeId + "\";</script>\n" +
           "<script>(function(){function renderBootError(title,detail){var root=document.getElementById('root');if(!root)return;root.innerHTML='<div "
           "style=\"padding:16px;max-width:900px;font-family:monospace;line-height:1.4;\">'+'<h2 style=\"margin:0 0 12px;color:#ffd5dd;\">'+title+'</h2>'+'<pre "
           "style=\"margin:0;white-space:pre-wrap;color:#ffd5dd;background:#2a1020;border:1px solid #7c2236;border-radius:8px;padding:12px;\">'+String(detail||'unknown error')+'</pre>'+'</div>'; } "
           "window.addEventListener('error',function(event){renderBootError('camflow ui javascript "
           "error',event&&event.message?event.message:event);});window.addEventListener('unhandledrejection',function(event){var "
           "reason=event&&event.reason?event.reason:event;renderBootError('camflow ui promise rejection',reason&&reason.message?reason.message:reason);});}());</script>\n";
}

} // namespace

WebInterface::WebInterface(std::string sourceNodeId) :
    m_sourceNodeId(std::move(sourceNodeId))
{
}

bool WebInterface::tryHandle(const std::string& method, const std::string& path, std::string& responseBody, std::string& contentType, int& statusCode) const
{
    if (m_sourceNodeId.empty()) {
        return false;
    }

    if (method != "GET") {
        return false;
    }

    const std::string requestPath = requestPathWithoutQuery(path);

    if (requestPath == "/" || requestPath == "/index.html") {
        statusCode = 200;
        contentType = "text/html; charset=utf-8";
        responseBody = renderPage();
        return true;
    }

    if (requestPath == "/assets/app.css" || requestPath == "/app.css") {
        statusCode = 200;
        contentType = "text/css; charset=utf-8";
        responseBody = camflow::webui::kUiAppCss;
        return true;
    }

    if (requestPath == "/assets/app.js" || requestPath == "/app.js") {
        statusCode = 200;
        contentType = "application/javascript; charset=utf-8";
        responseBody = camflow::webui::kUiAppJs;
        return true;
    }

    if (requestPath == "/api/version.md") {
        statusCode = 200;
        contentType = "text/markdown; charset=utf-8";
        responseBody = camflow::webui::kVersionMarkdown;
        return true;
    }

    return false;
}

std::string WebInterface::renderPage() const
{
    std::string html = camflow::webui::kUiIndexHtml;
    replaceAll(html, "</head>", injectedBootstrapMarkup(m_sourceNodeId) + "</head>");
    return html;
}
