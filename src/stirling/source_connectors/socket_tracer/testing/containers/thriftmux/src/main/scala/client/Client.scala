import com.twitter.util.{Await, Future}
import com.twitter.finagle.thriftmux.thriftscala.TestService
import com.twitter.finagle.ssl._
import com.twitter.finagle.ssl.client.SslClientConfiguration
import com.twitter.finagle.{Thrift, ThriftMux}

import java.io.File;

object Client {
  def main(args: Array[String]): Unit = {
    var useTls = false
    args.grouped(2).toList.collect {
      case Array("--use-tls", tls: String) => useTls = tls.toBoolean
    }
    val keyCredentials = KeyCredentials.CertAndKey(
      new File("/etc/ssl/client.crt"),
      new File("/etc/ssl/client.key"),
    )
    val sslConfig = SslClientConfiguration(
      keyCredentials=keyCredentials,
    )
    var stackClient = if (useTls) {
      ThriftMux.client
        .withLabel("thriftmux_example")
        .withTransport
        .tls(sslConfig)
    } else {
      ThriftMux.client
        .withLabel("thriftmux_example")
    }
    val svcPerEndpoint = stackClient
      .methodBuilder("inet!localhost:8080")
      .servicePerEndpoint[TestService.ServicePerEndpoint]
    val svc = ThriftMux.Client.methodPerEndpoint[TestService.ServicePerEndpoint, TestService.MethodPerEndpoint](svcPerEndpoint)
    val r = Await.result(svc.query("String"))
    println(r)
  }
}
