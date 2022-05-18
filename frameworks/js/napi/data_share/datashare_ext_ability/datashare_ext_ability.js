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

class DataShareExtensionAbility {
    onCreate(want) {
        console.log('onCreate, want:' + want.abilityName);
    }

    getFileTypes(uri, mimeTypeFilter) {
        console.log('getFileTypes, uri:' + uri);
    }

    openFile(uri, mode) {
        console.log('openFile, uri:' + uri);
    }

    openRawFile(uri, mode) {
        console.log('openRawFile, uri:' + uri);
    }

    insert(uri, value) {
        console.log('insert, uri:' + uri);
    }

    update(uri, predicates, value) {
        console.log('update, uri:' + uri);
    }

    delete(uri, predicates) {
        console.log('delete, uri:' + uri);
    }

    query(uri, predicates, columns) {
        console.log('query, uri:' + uri);
    }

    getType(uri) {
        console.log('getType, uri:' + uri);
    }

    batchInsert(uri, values) {
        console.log('batchInsert, uri:' + uri);
    }

    normalizeUri(uri) {
        console.log('normalizeUri, uri:' + uri);
    }

    denormalizeUri(uri) {
        console.log('denormalizeUri, uri:' + uri);
    }
}

export default DataShareExtensionAbility