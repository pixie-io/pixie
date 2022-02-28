import com.twitter.util.{Await, Future}
import com.twitter.finagle.thriftmux.thriftscala.TestService
import com.twitter.finagle.ssl._
import com.twitter.finagle.ssl.client.SslClientConfiguration
import com.twitter.finagle.{Thrift, ThriftMux}

import java.io.File;

object Client extends App {
  // TODO(ddelnano): Add flag that determines if TLS should be used rather than
  // hardcoding TLS being enabled
  val keyCredentials = KeyCredentials.CertAndKey(
    new File("/etc/ssl/client.crt"),
    new File("/etc/ssl/client.key"),
  )
  val sslConfig = SslClientConfiguration(
    None,
    keyCredentials,
  )
  val stackClient = ThriftMux.client
    .withLabel("thriftmux_example")
    .withTransport
    .tls(sslConfig)
  val svcPerEndpoint = stackClient
    .methodBuilder("inet!localhost:8080")
    .servicePerEndpoint[TestService.ServicePerEndpoint]
  val svc = ThriftMux.Client.methodPerEndpoint[TestService.ServicePerEndpoint, TestService.MethodPerEndpoint](svcPerEndpoint)
  val r = Await.result(svc.query("String"))
  println(r)
}
