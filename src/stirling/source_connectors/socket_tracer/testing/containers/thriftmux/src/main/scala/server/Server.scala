import com.twitter.finagle.{Http, Service}
import com.twitter.finagle.http
import com.twitter.finagle.{Thrift, ThriftMux}
import com.twitter.util.{Await, Future}
import java.net.{InetSocketAddress, InetAddress}
import com.twitter.finagle.thriftmux.thriftscala.TestService

object Server {
  def main(args: Array[String]): Unit = {
    val server = ThriftMux.server
      .serveIface(
        new InetSocketAddress(InetAddress.getLoopbackAddress, 8080),
        new TestService.MethodPerEndpoint {
          def query(x: String): Future[String] = Future.value(x + x)
          def question(y: String): Future[String] = Future.value(y + y)
          def inquiry(z: String): Future[String] = Future.value(z + z)
        },
      )
    Await.ready(server)
  }
}
