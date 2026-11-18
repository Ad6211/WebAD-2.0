#pragma once

#include <QWebEngineLoadingInfo>

struct NavigationError
{
    bool failed = false;
    int httpStatus = 0;
    QString message;
};

inline NavigationError navigationErrorFor(const QWebEngineLoadingInfo &info)
{
    const bool httpFailure =
        info.errorDomain() == QWebEngineLoadingInfo::HttpStatusCodeDomain
        && info.errorCode() >= 400;
    const bool navigationFailure =
        info.status() == QWebEngineLoadingInfo::LoadFailedStatus
        && info.errorDomain() != QWebEngineLoadingInfo::NoErrorDomain;
    if (!httpFailure && !navigationFailure) {
        return {};
    }
    return {true, httpFailure ? info.errorCode() : 0, info.errorString()};
}
