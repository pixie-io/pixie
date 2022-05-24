/*
 * Copyright 2018- The Pixie Authors.
 *
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { deserializeExecuteScriptRequest, deserializeExecuteScriptResponse } from './grpc';

/* eslint-disable max-len */

const ExecuteScriptRequestString = ''
 + 'AAAAA3MKKWltcG9ydCBweApweC5kaXNwbGF5KHB4LkdldEFnZW50U3RhdHVzKCkpGiQ2ODExNzdiZC04ZTkyLTRjODAtYTNmOS0zOTNiZTg5YTE4M2UynwYKgAZ7ImFsZyI6IlJTQS1PQUVQLTI1NiIsImUiOiJBUUFCIiwiZXh0Ijp0cnVlLCJrZXlfb3BzIjpbImVuY3J5cHQiXSwia3R5IjoiUlNBIiwibiI6IjRjdVMzSURNemh0OFhXLU5Pd3l4bXZGTDBLYlFJbjU4ekttSDAtRHpZTmZlbjZNMXZIUGV2RVFZVVlrRlk4ZGhFaG4zMjFiNm1FX3Jlc0JUUTJLVkExQzFNR1NSQUlMNFRzWTB4eFJ2VFR2QUZybVNPVXh4Z2dlby1aVW5nVzZHT2NadDZYRXlzS2tBeDhUTEJab1VJOWNZQUpyZjZpeHFKM0Y3YkNRTVJHTi1rV0N0emtDRlpXb2JhQ2J6YWsxM3dVQjA4MVJBNzYwTEFMeEd0dVprMktsMGV3OWJ1Z1Z4dU1xS1FIUGJONEhKYUl1ck1BYmxRdTBsTktSTXUwS0RSaVN4LWVPYTU3MmRTVHhFV2I0N0RqTjNGQXFwT3JKbUxXTkFNX3E4Tzl0LTdyQmxTN090QUFLSTBfVWxMZzVWWGRra2diQkVkU3J3RktEdTdVVmlnM0xJMEtVZzNRNTFJaGJoM2FsMU9kWkpLZURNTmk0c1M5aGc1dHNqSm1XMzN0eWJ2Y1pQYVg1elUwUVo2SjdyMFVHSGc4a2dOTXM5QmhVMXlDM3dZU0s0b0o5Q0NzTWV2aDZwMnRnQkw4OXZhRnRiaEtyblYtNzZ1eVZFbUlJNmU2TC12VFJyZVNaYTBlUmlxWDZkWHczdnFKSl9DRHNjY3dqYUtkYWJBY25lYnc3NFJUcjhPSF95NDAtVTJ0eEtCSFdVWVIxR0h3a3c4cnBzYTZWWWhRV0o4VzgxZ0ZfeklDRS1sZzJCZDZDT19mNi1xTV9ERFRXaUhCdE1DMUgyejNFYjBNb25lOG16ckRCbWpjenNjbVNDLXFiRVhKbU9LNDNucVZxaVlCdHNKUWgwOG5kZmZwY3FBZ3UwSmd5dElQZFF3X0hXdTN0NVZnejdISU52YV9zIn0SDFJTQS1PQUVQLTI1NhoHQTI1NkdDTSIDREVG';

const ExecuteScriptRequestJson = {
  'queryStr': [
    'import px',
    'px.display(px.GetAgentStatus())',
  ].join('\n'),
  'clusterId': '681177bd-8e92-4c80-a3f9-393be89a183e',
  'execFuncsList': [],
  'mutation': false,
  'encryptionOptions': {
    'jwkKey': '{"alg":"RSA-OAEP-256","e":"AQAB","ext":true,"key_ops":["encrypt"],"kty":"RSA","n":"4cuS3IDMzht8XW-NOwyxmvFL0KbQIn58zKmH0-DzYNfen6M1vHPevEQYUYkFY8dhEhn321b6mE_resBTQ2KVA1C1MGSRAIL4TsY0xxRvTTvAFrmSOUxxggeo-ZUngW6GOcZt6XEysKkAx8TLBZoUI9cYAJrf6ixqJ3F7bCQMRGN-kWCtzkCFZWobaCbzak13wUB081RA760LALxGtuZk2Kl0ew9bugVxuMqKQHPbN4HJaIurMAblQu0lNKRMu0KDRiSx-eOa572dSTxEWb47DjN3FAqpOrJmLWNAM_q8O9t-7rBlS7OtAAKI0_UlLg5VXdkkgbBEdSrwFKDu7UVig3LI0KUg3Q51Ihbh3al1OdZJKeDMNi4sS9hg5tsjJmW33tybvcZPaX5zU0QZ6J7r0UGHg8kgNMs9BhU1yC3wYSK4oJ9CCsMevh6p2tgBL89vaFtbhKrnV-76uyVEmII6e6L-vTRreSZa0eRiqX6dXw3vqJJ_CDsccwjaKdabAcnebw74RTr8OH_y40-U2txKBHWUYR1GHwkw8rpsa6VYhQWJ8W81gF_zICE-lg2Bd6CO_f6-qM_DDTWiHBtMC1H2z3Eb0Mone8mzrDBmjczscmSC-qbEXJmOK43nqVqiYBtsJQh08ndffpcqAgu0JgytIPdQw_HWu3t5Vgz7HINva_s"}',
    'keyAlg': 'RSA-OAEP-256',
    'contentAlg': 'A256GCM',
    'compressionAlg': 'DEF',
  },
  'queryId': '',
  'queryName': '',
};

const ExecuteScriptResponseString = ''
 + 'AAAAANYSJDIxOTA1YjA1LTIxYjctNDNmMy1hNmI5LTViYWQ4NDFmMTNjZCKtAQp9Cg4KCGFnZW50X2lkEAMgAQoKCgRhc2lkEAIgAQoOCghob3N0bmFtZRAFIAEKEAoKaXBfYWRkcmVzcxAFIAEKEQoLYWdlbnRfc3RhdGUQBSABChEKC2NyZWF0ZV90aW1lEAYgAQoXChFsYXN0X2hlYXJ0YmVhdF9ucxACIAESBm91dHB1dBokYzVhNjQzNWItYWM0MC00MjUyLWI0ZWItOGMxMjJlZjZhMTRkAAAABTQSJDIxOTA1YjA1LTIxYjctNDNmMy1hNmI5LTViYWQ4NDFmMTNjZBqLChqICmV5SmhiR2NpT2lKU1UwRXRUMEZGVUMweU5UWWlMQ0psYm1NaU9pSkJNalUyUjBOTklpd2llbWx3SWpvaVJFVkdJbjAuZEJXUkJ6OVUyYmplcEstYmx0VUhtaTVuUzZObmM4Y2VWQzl3ME1OTUQtbDdVWjZUTDJ0d1MxVUdGYUphNUVKMC1jZjQ3dGpBRlVOSXYxbklkenFEdVFOa3Q4SElMTnpaR0JvSzhIeDg1ZXhFMXByVDhNajUzYi15UC1valR1ZzJJQThoWTZBUUx2Mkh3RklxVkczRVY5Q3I3bUZxRUw0YWRiZFcydlAzc0tDWTNtR01PRC1feHFDZjhmTFFteWtWTDB4UHVxbm1CYnJZUWctdzRRZUZCLXFrTzZZWC1hRmV6U29XSTY1WXI2cFQ4dDVfYkdKcW02amFsSW1wakhkSERmbHBGLU9jSzlpZnFKYUpSbTZvcDVLbWdnZDh0S0dhcVlSRW9vTzVwekdJdWh0cEpMNGxFbUJwNlR1UmlISlZWSWZTTTJ5RU9LWVp3QkV4YmozRHdOWUVkNEN4aV8ybUE4aFg0UHFKSlRlSXFqMGY4UFZRWERJRHFCY2RpOS15QjQ0VDI5LWNWTVpkVWtSR1F2QlhubnJUWktnMWtCTGEtMVF4a0gxTTFvUF8xSUkyS1RzVl9qOTd5QnUxUXBMMURRZmRROHlGNGlxVXJPdTcwM0Y5RE9ETjQ1TlBkYTN4OFNCWTc5LXg4ZG0weUxQTjNvcnBuOG1vWXlDLW96NjAzX3o4dlREeExZWEZsYXVDd2kxbHRsWXQ1UDU1NUVsR2NiV2ppeHo2NFBSTHlDZHRyaV9wdHhXcHNnLTBhNzNYbmNOX2piMHNKdHdwOERVakFUUVdqdEVPZ1pVVHhwSy1zOVpMN29UU1RHT2hMejdRNjJ4VnpMVldBTlVVYnVUMVJCODVrdGI4TUxucG42VndJZXY4d0R0ckRRMy1CVHBjWDJjSjNjdkRwWFUuWGVsMEpFRFd5eFlHYnhPaS5ISWczVmxodzhYQk5Qd2FqYUt5SlpuMVRHSzZlc2pKR1MwNzU2M29BVEtSV3NidWJJeTNaNUFiZ0M3VlZHUkxCWFFZdFoybFE2ZnpOTU1xSl9wVS1HM1RMZldET0JvOGJ5Ui1wZVBNMUhyUmdTdldlSnBRRnhiM0JIWmpOb0RKNWNvZnVIQ1VkcEVheGFiRlphQ3ZOWFhJUV8zbXFMTmQ2bmFISXhrTTJRbmhwQ1F3aHFRRjBKVHo0WlVTQnk1VElzOW5oYUx3OUliaUVpTlk5S2wzeU5OLWdtSUlfTWp6eXExQ3owQ2NnVXFKOTFKMC1YUVFOV0NFY2tNbWVEZ1A2V0gyNmFuaVFoYks2OFRFdkVCZ1REUUd1RHR5QlRoR2VqZWhwNlAxbl80Z3kzT01jWldPNVhOTGI1cW1KOFdvb2lVRDZzcVRSVGtkd1VUd204cGYtVWtiSGE4Wnl1WTZ0c1QzQUZHaTVUNldVS2VWWDM2UzkwNUdDRkd1cFNTMUZ1UHZnQy1FeW1rSFJRdFhDMk9XZFZmWjBoUFhlazlGV19OT045ODhYNUlycXdmdWNpT3pYMWxjMHJNS01mSWhQV0tqOGpJZktkV1BCeV81Rnc3S0FZbGNoT01MbkgtcDM3Rkp1MGRhRGlwQ2NzbjZjLkdnVEhTWWlfdFB4UTJxdDZBRldsSGc=AAAAADYSJDIxOTA1YjA1LTIxYjctNDNmMy1hNmI5LTViYWQ4NDFmMTNjZBoOEgwKCgjo96UDEK+KnAE=gAAAAA9ncnBjLXN0YXR1czowDQo=';

const ExecuteScriptResponseJson = [
  {
    'status': undefined,
    'data': undefined,
    'mutationInfo': undefined,
    'queryId': '21905b05-21b7-43f3-a6b9-5bad841f13cd',
    'metaData': {
      'relation': {
        'columnsList': [
          {
            'columnName': 'agent_id',
            'columnType': 3,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'asid',
            'columnType': 2,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'hostname',
            'columnType': 5,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'ip_address',
            'columnType': 5,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'agent_state',
            'columnType': 5,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'create_time',
            'columnType': 6,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
          {
            'columnName': 'last_heartbeat_ns',
            'columnType': 2,
            'columnDesc': '',
            'columnSemanticType': 1,
          },
        ],
      },
      'name': 'output',
      'id': 'c5a6435b-ac40-4252-b4eb-8c122ef6a14d',
    },
  },
  {
    'status': undefined,
    'metaData': undefined,
    'mutationInfo': undefined,
    'queryId': '21905b05-21b7-43f3-a6b9-5bad841f13cd',
    'data': {
      'batch': undefined,
      'executionStats': undefined,
      'encryptedBatch': 'ZXlKaGJHY2lPaUpTVTBFdFQwRkZVQzB5TlRZaUxDSmxibU1pT2lKQk1qVTJSME5OSWl3aWVtbHdJam9pUkVWR0luMC5kQldSQno5VTJiamVwSy1ibHRVSG1pNW5TNk5uYzhjZVZDOXcwTU5NRC1sN1VaNlRMMnR3UzFVR0ZhSmE1RUowLWNmNDd0akFGVU5JdjFuSWR6cUR1UU5rdDhISUxOelpHQm9LOEh4ODVleEUxcHJUOE1qNTNiLXlQLW9qVHVnMklBOGhZNkFRTHYySHdGSXFWRzNFVjlDcjdtRnFFTDRhZGJkVzJ2UDNzS0NZM21HTU9ELV94cUNmOGZMUW15a1ZMMHhQdXFubUJicllRZy13NFFlRkItcWtPNllYLWFGZXpTb1dJNjVZcjZwVDh0NV9iR0pxbTZqYWxJbXBqSGRIRGZscEYtT2NLOWlmcUphSlJtNm9wNUttZ2dkOHRLR2FxWVJFb29PNXB6R0l1aHRwSkw0bEVtQnA2VHVSaUhKVlZJZlNNMnlFT0tZWndCRXhiajNEd05ZRWQ0Q3hpXzJtQThoWDRQcUpKVGVJcWowZjhQVlFYRElEcUJjZGk5LXlCNDRUMjktY1ZNWmRVa1JHUXZCWG5uclRaS2cxa0JMYS0xUXhrSDFNMW9QXzFJSTJLVHNWX2o5N3lCdTFRcEwxRFFmZFE4eUY0aXFVck91NzAzRjlET0RONDVOUGRhM3g4U0JZNzkteDhkbTB5TFBOM29ycG44bW9ZeUMtb3o2MDNfejh2VER4TFlYRmxhdUN3aTFsdGxZdDVQNTU1RWxHY2JXaml4ejY0UFJMeUNkdHJpX3B0eFdwc2ctMGE3M1huY05famIwc0p0d3A4RFVqQVRRV2p0RU9nWlVUeHBLLXM5Wkw3b1RTVEdPaEx6N1E2MnhWekxWV0FOVVVidVQxUkI4NWt0YjhNTG5wbjZWd0lldjh3RHRyRFEzLUJUcGNYMmNKM2N2RHBYVS5YZWwwSkVEV3l4WUdieE9pLkhJZzNWbGh3OFhCTlB3YWphS3lKWm4xVEdLNmVzakpHUzA3NTYzb0FUS1JXc2J1Ykl5M1o1QWJnQzdWVkdSTEJYUVl0WjJsUTZmek5NTXFKX3BVLUczVExmV0RPQm84YnlSLXBlUE0xSHJSZ1N2V2VKcFFGeGIzQkhaak5vREo1Y29mdUhDVWRwRWF4YWJGWmFDdk5YWElRXzNtcUxOZDZuYUhJeGtNMlFuaHBDUXdocVFGMEpUejRaVVNCeTVUSXM5bmhhTHc5SWJpRWlOWTlLbDN5Tk4tZ21JSV9Nanp5cTFDejBDY2dVcUo5MUowLVhRUU5XQ0Vja01tZURnUDZXSDI2YW5pUWhiSzY4VEV2RUJnVERRR3VEdHlCVGhHZWplaHA2UDFuXzRneTNPTWNaV081WE5MYjVxbUo4V29vaVVENnNxVFJUa2R3VVR3bThwZi1Va2JIYThaeXVZNnRzVDNBRkdpNVQ2V1VLZVZYMzZTOTA1R0NGR3VwU1MxRnVQdmdDLUV5bWtIUlF0WEMyT1dkVmZaMGhQWGVrOUZXX05PTjk4OFg1SXJxd2Z1Y2lPelgxbGMwck1LTWZJaFBXS2o4aklmS2RXUEJ5XzVGdzdLQVlsY2hPTUxuSC1wMzdGSnUwZGFEaXBDY3NuNmMuR2dUSFNZaV90UHhRMnF0NkFGV2xIZw==',
    },
  },
  {
    'status': undefined,
    'metaData': undefined,
    'mutationInfo': undefined,
    'queryId': '21905b05-21b7-43f3-a6b9-5bad841f13cd',
    'data': {
      'batch': undefined,
      'encryptedBatch': '',
      'executionStats': {
        'timing': {
          'executionTimeNs': 6913000,
          'compilationTimeNs': 2557231,
        },
        'bytesProcessed': 0,
        'recordsProcessed': 0,
      },
    },
  },
];

describe('Script execution helpers', () => {
  it('Deserializes a request correctly', () => {
    const json = deserializeExecuteScriptRequest(ExecuteScriptRequestString).toObject();
    expect(json).toEqual(ExecuteScriptRequestJson);
  });

  it('Deserializes a response correctly', () => {
    const messages = deserializeExecuteScriptResponse(ExecuteScriptResponseString).map((m) => m.toObject());
    expect(messages.length).toBe(ExecuteScriptResponseJson.length);
    for (let i = 0; i < ExecuteScriptResponseJson.length; i++) {
      expect(messages[i]).toEqual(ExecuteScriptResponseJson[i]);
    }
  });
});
