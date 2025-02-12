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

const http = require('http')
const url = require('url')
const port = process.env.PORT || 8080
const crypto = require('crypto')

const id = crypto.randomBytes(4).toString('hex')
const message = crypto.randomBytes(32).toString('hex').substr(0,63)+"\n"
const len = message.length
const targets = []
const isDebug = process.env.DEBUG === 'true'
const success = parseInt(process.env.SUCCESS_PERCENT,10) || 90
const errorCodes = [400, 401, 403, 418, 422, 500]

for ( const k of Object.keys(process.env) ) {
  if ( k.startsWith('TARGET_') ) {
    let [host, port, bytes, seconds] = process.env[k].split(/[:,]/)
    targets.push({host, port, bytes, seconds})
  }
}

console.log(`[${id}] listening on ${port}`)
for ( const t of targets ) {
  debug(`Request ${t.bytes} from ${t.host}:${t.port} every ${t.seconds} seconds`)
  setInterval(() => {
    const req = http.request(`http://${t.host}:${t.port}/${id}/${t.bytes}`, (res) => {
      let received = 0
      let fromId = ''

      res.on('data', (chunk) => {
        if ( !received ) {
          fromId = chunk.toString().split(' ')[0]
        }
        received += chunk.length
      })

      res.on('end', () => {
        debug(`[${id}] Received ${received} from ${fromId} (${t.host}:${t.port})`)
      })
    })

    req.on('error', (err) => {
      console.error(`[${id}] Error: ${err}`)
    })

    req.end()
  }, t.seconds * 1000)
}
console.log('===============================')

http.createServer((req, res) => {
  const parsed = url.parse(req.url)
  const parts = parsed.path.substring(1).split('/')
  let target = parts[0]
  let bytes = parseInt(parts[1],10) || 4096
  let status = 200

  if ( crypto.randomInt(0, 101) > success ) {
    status = errorCodes[crypto.randomInt(0,errorCodes.length)]
  }

  res.writeHead(status, {'Content-Type': 'text/plain'})
  const intro = `${id} ${status} ${bytes} ${message}\n`
  bytes -= intro.length
  res.write(intro)

  while ( bytes > len ) {
    res.write(message)
    bytes -= len
  }

  if ( bytes > 0 ) {
    res.write(message.substring(0,bytes))
  }

  debug(`[${id}] Sent ${bytes} to ${target}`)
  res.end()
}).listen(port)

function debug(...msg) {
  if ( isDebug ) {
    console.log(...msg)
  }
}
