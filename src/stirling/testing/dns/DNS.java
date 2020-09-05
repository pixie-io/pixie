import java.io.*;
import java.net.*;
import java.util.Random;

class DNS
{
  public static void main(String args[]) throws InterruptedException
  {
    while (true) {
      try {
        String site = RandomSite();
        InetAddress address = InetAddress.getByName(RandomSite());
        System.out.println(site + ": " + address.getHostAddress());
      } catch (Exception e) {
        System.out.println(e);
      }
      Thread.sleep(3000);
    }
  }

  public static String RandomSite() {
    String alphabet = "abcdefghijklmnopqrstuvwxyz";

    String site = "";
    for (int i = 0; i < 3; i++) {
      site += alphabet.charAt(random.nextInt(alphabet.length()));
    }

    return "www." + site + ".com";
  }

  static Random random = new Random();
}
