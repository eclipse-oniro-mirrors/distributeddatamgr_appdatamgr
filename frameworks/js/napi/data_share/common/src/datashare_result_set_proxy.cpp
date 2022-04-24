/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "datashare_result_set_proxy.h"

#include <functional>

#include "datashare_result_set.h"
#include "datashare_js_utils.h"
#include "napi_datashare_async_proxy.h"
#include "string_ex.h"
#include "datashare_log.h"

namespace OHOS {
namespace DataShare {
static napi_ref __thread ctorRef_ = nullptr;
static const int E_OK = 0;
napi_value DataShareResultSetProxy::NewInstance(napi_env env, std::shared_ptr<DataShareResultSet> resultSet)
{
    napi_value cons = GetConstructor(env);
    if (cons == nullptr) {
        LOG_ERROR("NewInstance GetConstructor is nullptr!");
        return nullptr;
    }
    napi_value instance;
    napi_status status = napi_new_instance(env, cons, 0, nullptr, &instance);
    if (status != napi_ok) {
        LOG_ERROR("NewInstance napi_new_instance failed! code:%{public}d!", status);
        return nullptr;
    }

    DataShareResultSetProxy *proxy = nullptr;
    status = napi_unwrap(env, instance, reinterpret_cast<void **>(&proxy));
    if (proxy == nullptr) {
        LOG_ERROR("NewInstance native instance is nullptr! code:%{public}d!", status);
        return instance;
    }

    if (resultSet->GetBlock() != nullptr) {
        proxy->sharedBlockName_ = resultSet->GetBlock()->Name();
        proxy->sharedBlockAshmemFd_ = resultSet->GetBlock()->GetFd();
    }
    *proxy = std::move(resultSet);
    return instance;
}

std::shared_ptr<DataShareResultSet> DataShareResultSetProxy::GetNativeObject(
    napi_env const &env, napi_value const &arg)
{
    if (arg == nullptr) {
        LOG_ERROR("DataShareResultSetProxy GetNativeObject arg is null.");
        return nullptr;
    }
    DataShareResultSetProxy *proxy = nullptr;
    napi_unwrap(env, arg, reinterpret_cast<void **>(&proxy));
    if (proxy == nullptr) {
        LOG_ERROR("DataShareResultSetProxy GetNativeObject proxy is null.");
        return nullptr;
    }
    return proxy->resultSet_;
}

napi_value DataShareResultSetProxy::GetConstructor(napi_env env)
{
    napi_value cons;
    if (ctorRef_ != nullptr) {
        NAPI_CALL(env, napi_get_reference_value(env, ctorRef_, &cons));
        return cons;
    }
    LOG_INFO("GetConstructor result set constructor");
    napi_property_descriptor clzDes[] = {
        DECLARE_NAPI_FUNCTION("goToFirstRow", GoToFirstRow),
        DECLARE_NAPI_FUNCTION("goToLastRow", GoToLastRow),
        DECLARE_NAPI_FUNCTION("goToNextRow", GoToNextRow),
        DECLARE_NAPI_FUNCTION("goToPreviousRow", GoToPreviousRow),
        DECLARE_NAPI_FUNCTION("goTo", GoTo),
        DECLARE_NAPI_FUNCTION("goToRow", GoToRow),
        DECLARE_NAPI_FUNCTION("getBlob", GetBlob),
        DECLARE_NAPI_FUNCTION("getString", GetString),
        DECLARE_NAPI_FUNCTION("getInt", GetInt),
        DECLARE_NAPI_FUNCTION("getLong", GetLong),
        DECLARE_NAPI_FUNCTION("getDouble", GetDouble),
        DECLARE_NAPI_FUNCTION("isColumnNull", IsColumnOrKeyNull),
        DECLARE_NAPI_FUNCTION("close", Close),
        DECLARE_NAPI_FUNCTION("getColumnOrKeyIndex", GetColumnOrKeyIndex),
        DECLARE_NAPI_FUNCTION("getColumnOrKeyName", GetColumnOrKeyName),
        DECLARE_NAPI_FUNCTION("getDataType", GetDataType),

        DECLARE_NAPI_GETTER("columnOrKeyNames", GetAllColumnOrKeyNames),
        DECLARE_NAPI_GETTER("columnOrKeyCount", GetColumnOrKeyCount),
        DECLARE_NAPI_GETTER("rowCount", GetRowCount),
        DECLARE_NAPI_GETTER("rowIndex", GetRowIndex),
        DECLARE_NAPI_GETTER("isAtFirstRow", IsAtFirstRow),
        DECLARE_NAPI_GETTER("isAtLastRow", IsAtLastRow),
        DECLARE_NAPI_GETTER("isEnded", IsEnded),
        DECLARE_NAPI_GETTER("isStarted", IsStarted),
        DECLARE_NAPI_GETTER("isClosed", IsClosed),

        DECLARE_NAPI_GETTER("sharedBlockName", GetSharedBlockName),
        DECLARE_NAPI_GETTER("sharedBlockAshmemFd", GetSharedBlockAshmemFd),
    };
    NAPI_CALL(env, napi_define_class(env, "ResultSet", NAPI_AUTO_LENGTH, Initialize, nullptr,
        sizeof(clzDes) / sizeof(napi_property_descriptor), clzDes, &cons));
    NAPI_CALL(env, napi_create_reference(env, cons, 1, &ctorRef_));
    return cons;
}

napi_value DataShareResultSetProxy::Initialize(napi_env env, napi_callback_info info)
{
    napi_value self = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &self, nullptr));
    auto *proxy = new DataShareResultSetProxy();
    auto finalize = [](napi_env env, void *data, void *hint) {
        DataShareResultSetProxy *proxy = reinterpret_cast<DataShareResultSetProxy *>(data);
        delete proxy;
    };
    napi_status status = napi_wrap(env, self, proxy, finalize, nullptr, nullptr);
    if (status != napi_ok) {
        LOG_ERROR("DataShareResultSetProxy napi_wrap failed! code:%{public}d!", status);
        finalize(env, proxy, nullptr);
        return nullptr;
    }
    return self;
}

DataShareResultSetProxy::~DataShareResultSetProxy()
{
    LOG_INFO("DataShareResultSetProxy destructor!");
    if (resultSet_ != nullptr && !resultSet_->IsClosed()) {
        resultSet_->Close();
    }
}

DataShareResultSetProxy::DataShareResultSetProxy(std::shared_ptr<DataShareResultSet> resultSet)
{
    if (resultSet_ == resultSet) {
        return;
    }
    resultSet_ = std::move(resultSet);
}

DataShareResultSetProxy &DataShareResultSetProxy::operator=(std::shared_ptr<DataShareResultSet> resultSet)
{
    if (resultSet_ == resultSet) {
        return *this;
    }
    resultSet_ = std::move(resultSet);
    return *this;
}

std::shared_ptr<DataShareResultSet> &DataShareResultSetProxy::GetInnerResultSet(napi_env env,
    napi_callback_info info)
{
    DataShareResultSetProxy *resultSet = nullptr;
    napi_value self = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &self, nullptr);
    napi_unwrap(env, self, reinterpret_cast<void **>(&resultSet));
    return resultSet->resultSet_;
}

napi_value DataShareResultSetProxy::GoToFirstRow(napi_env env, napi_callback_info info)
{
    int errCode = GetInnerResultSet(env, info)->GoToFirstRow();
    if (errCode != E_OK) {
        LOG_ERROR("GoToFirstRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, (errCode == E_OK));
}

napi_value DataShareResultSetProxy::GoToLastRow(napi_env env, napi_callback_info info)
{
    int errCode = GetInnerResultSet(env, info)->GoToLastRow();
    if (errCode != E_OK) {
        LOG_ERROR("GoToLastRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, (errCode == E_OK));
}

napi_value DataShareResultSetProxy::GoToNextRow(napi_env env, napi_callback_info info)
{
    int errCode = GetInnerResultSet(env, info)->GoToNextRow();
    if (errCode != E_OK) {
        LOG_ERROR("GoToNextRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, (errCode == E_OK));
}

napi_value DataShareResultSetProxy::GoToPreviousRow(napi_env env, napi_callback_info info)
{
    int errCode = GetInnerResultSet(env, info)->GoToPreviousRow();
    if (errCode != E_OK) {
        LOG_ERROR("GoToPreviousRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, (errCode == E_OK));
}

napi_value DataShareResultSetProxy::GoTo(napi_env env, napi_callback_info info)
{
    int32_t offset = -1;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &offset));
    int errCode = GetInnerResultSet(env, info)->GoTo(offset);
    if (errCode != E_OK) {
        LOG_ERROR("GoTo failed code:%{public}d", errCode);
    }
    napi_value output;
    napi_get_undefined(env, &output);
    return output;
}

napi_value DataShareResultSetProxy::GoToRow(napi_env env, napi_callback_info info)
{
    int32_t position = -1;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &position));
    int errCode = GetInnerResultSet(env, info)->GoToRow(position);
    if (errCode != E_OK) {
        LOG_ERROR("GoToRow failed code:%{public}d", errCode);
    }
    napi_value output;
    napi_get_undefined(env, &output);
    return output;
}

napi_value DataShareResultSetProxy::GetBlob(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    std::vector<uint8_t> blob;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = GetInnerResultSet(env, info)->GetBlob(columnOrkeyIndex, blob);
    if (errCode != E_OK) {
        LOG_ERROR("GetBlob failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, blob);
}

napi_value DataShareResultSetProxy::GetString(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    std::string value;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = GetInnerResultSet(env, info)->GetString(columnOrkeyIndex, value);
    if (errCode != E_OK) {
        LOG_ERROR("GetString failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, value);
}

napi_value DataShareResultSetProxy::GetInt(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    int32_t value;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = GetInnerResultSet(env, info)->GetInt(columnOrkeyIndex, value);
    if (errCode != E_OK) {
        LOG_ERROR("GetInt failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, value);
}

napi_value DataShareResultSetProxy::GetLong(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    int64_t value = -1;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = GetInnerResultSet(env, info)->GetLong(columnOrkeyIndex, value);
    if (errCode != E_OK) {
        LOG_ERROR("GetLong failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, value);
}

napi_value DataShareResultSetProxy::GetDouble(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    double value = 0.0;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = GetInnerResultSet(env, info)->GetDouble(columnOrkeyIndex, value);
    if (errCode != E_OK) {
        LOG_ERROR("GetDouble failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, value);
}

napi_value DataShareResultSetProxy::IsColumnOrKeyNull(napi_env env, napi_callback_info info)
{
    int32_t columnOrkeyIndex = -1;
    bool isNull = false;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrkeyIndex));
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->IsColumnOrKeyNull(columnOrkeyIndex, isNull);
    if (errCode != E_OK) {
        LOG_ERROR("IsColumnNull failed code:%{public}d", errCode);
    }
    napi_value output;
    napi_get_boolean(env, isNull, &output);
    return output;
}

napi_value DataShareResultSetProxy::Close(napi_env env, napi_callback_info info)
{
    int errCode = GetInnerResultSet(env, info)->Close();
    if (errCode != E_OK) {
        LOG_ERROR("Close failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, (errCode == E_OK));
}

napi_value DataShareResultSetProxy::GetColumnOrKeyIndex(napi_env env, napi_callback_info info)
{
    int32_t columnOrKeyIndex = -1;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    std::string columnOrKeyName = DataShareJSUtils::Convert2String(env, args[0], DataShareJSUtils::DEFAULT_BUF_SIZE);
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->GetColumnOrKeyIndex(columnOrKeyName, columnOrKeyIndex);
    if (errCode != E_OK) {
        LOG_ERROR("GetColumnIndex failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, columnOrKeyIndex);
}

napi_value DataShareResultSetProxy::GetColumnOrKeyName(napi_env env, napi_callback_info info)
{
    int32_t columnOrKeyIndex = -1;
    std::string columnOrKeyName;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrKeyIndex));
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->GetColumnOrKeyName(columnOrKeyIndex, columnOrKeyName);
    if (errCode != E_OK) {
        LOG_ERROR("GetColumnName failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, columnOrKeyName);
}

napi_value DataShareResultSetProxy::GetDataType(napi_env env, napi_callback_info info)
{
    int32_t columnOrKeyIndex = -1;
    DataType dataType = DataType::TYPE_NULL;
    size_t argc = MAX_INPUT_COUNT;
    napi_value args[MAX_INPUT_COUNT] = { 0 };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    NAPI_ASSERT(env, argc > 0, "Invalid argvs!");
    NAPI_CALL(env, napi_get_value_int32(env, args[0], &columnOrKeyIndex));
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->GetDataType(columnOrKeyIndex, dataType);
    if (errCode != E_OK) {
        LOG_ERROR("GetColumnType failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, int32_t(dataType));
}

napi_value DataShareResultSetProxy::GetAllColumnOrKeyNames(napi_env env, napi_callback_info info)
{
    std::vector<std::string> columnOrKeyNames;
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->GetAllColumnOrKeyNames(columnOrKeyNames);
    if (errCode != E_OK) {
        LOG_ERROR("GetAllColumnNames failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, columnOrKeyNames);
}

napi_value DataShareResultSetProxy::GetColumnOrKeyCount(napi_env env, napi_callback_info info)
{
    int32_t count = -1;
    int errCode = E_OK;
    // int errCode = GetInnerResultSet(env, info)->GetColumnOrKeyCount(count);
    if (errCode != E_OK) {
        LOG_ERROR("GetColumnCount failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, count);
}

napi_value DataShareResultSetProxy::GetRowCount(napi_env env, napi_callback_info info)
{
    int32_t count = -1;
    int errCode = GetInnerResultSet(env, info)->GetRowCount(count);
    if (errCode != E_OK) {
        LOG_ERROR("GetRowCount failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, count);
}

napi_value DataShareResultSetProxy::GetRowIndex(napi_env env, napi_callback_info info)
{
    int32_t position = -1;
    int errCode = GetInnerResultSet(env, info)->GetRowIndex(position);
    if (errCode != E_OK) {
        LOG_ERROR("GetRowIndex failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, position);
}

napi_value DataShareResultSetProxy::IsAtFirstRow(napi_env env, napi_callback_info info)
{
    bool result = false;
    int errCode = GetInnerResultSet(env, info)->IsAtFirstRow(result);
    if (errCode != E_OK) {
        LOG_ERROR("IsAtFirstRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, result);
}

napi_value DataShareResultSetProxy::IsAtLastRow(napi_env env, napi_callback_info info)
{
    bool result = false;
    int errCode = GetInnerResultSet(env, info)->IsAtLastRow(result);
    if (errCode != E_OK) {
        LOG_ERROR("IsAtLastRow failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, result);
}

napi_value DataShareResultSetProxy::IsEnded(napi_env env, napi_callback_info info)
{
    bool result = false;
    int errCode = GetInnerResultSet(env, info)->IsEnded(result);
    if (errCode != E_OK) {
        LOG_ERROR("IsEnded failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, result);
}

napi_value DataShareResultSetProxy::IsStarted(napi_env env, napi_callback_info info)
{
    bool result = false;
    int errCode = GetInnerResultSet(env, info)->IsStarted(result);
    if (errCode != E_OK) {
        LOG_ERROR("IsBegin failed code:%{public}d", errCode);
    }
    return DataShareJSUtils::Convert2JSValue(env, result);
}

napi_value DataShareResultSetProxy::IsClosed(napi_env env, napi_callback_info info)
{
    bool result = GetInnerResultSet(env, info)->IsClosed();
    napi_value output;
    napi_get_boolean(env, result, &output);
    return output;
}

napi_value DataShareResultSetProxy::GetSharedBlockName(napi_env env, napi_callback_info info)
{
    napi_value thiz;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thiz, nullptr));

    DataShareResultSetProxy *proxy;
    NAPI_CALL(env, napi_unwrap(env, thiz, reinterpret_cast<void **>(&proxy)));

    return DataShareJSUtils::Convert2JSValue(env, proxy->sharedBlockName_);
}

napi_value DataShareResultSetProxy::GetSharedBlockAshmemFd(napi_env env, napi_callback_info info)
{
    napi_value thiz;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thiz, nullptr));

    DataShareResultSetProxy *proxy;
    NAPI_CALL(env, napi_unwrap(env, thiz, reinterpret_cast<void **>(&proxy)));

    return DataShareJSUtils::Convert2JSValue(env, proxy->sharedBlockAshmemFd_);
}
} // namespace DataShare
} // namespace OHOS
napi_value OHOS::DataShare::GetNapiResultSetObject(napi_env env, OHOS::DataShare::DataShareResultSet *resultSet)
{
    return OHOS::DataShare::DataShareResultSetProxy::NewInstance(
        env, std::shared_ptr<OHOS::DataShare::DataShareResultSet>(resultSet));
}
OHOS::DataShare::DataShareResultSet *OHOS::DataShare::GetResultSetProxyObject(const napi_env &env, const napi_value &arg)
{
    // the resultSet maybe release.
    auto resultSet = OHOS::DataShare::DataShareResultSetProxy::GetNativeObject(env, arg);
    return resultSet.get();
}
