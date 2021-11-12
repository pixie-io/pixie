import com.twitter.util.{Await, Future}
import com.twitter.finagle.thriftmux.thriftscala.TestService
import com.twitter.finagle.{Thrift, ThriftMux}

object Client extends App {
  val stackClient = ThriftMux.client.withLabel("thriftmux_example")
  val svcPerEndpoint = stackClient
    .methodBuilder("inet!localhost:8080")
    .servicePerEndpoint[TestService.ServicePerEndpoint]
  val svc = ThriftMux.Client.methodPerEndpoint[TestService.ServicePerEndpoint, TestService.MethodPerEndpoint](svcPerEndpoint)
  val r = Await.result(svc.query("String"))
  println(r)
}
