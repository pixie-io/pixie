public class HelloWorld {

    public static void main(String[] args) throws InterruptedException {
        System.out.println("Hello, World");
        // The purpose of this program is to write a memory-mapped file at
        // /tmp/hsperfdata_<user>/<pid> to verify the JVM stats collecting feature.
        // Sleep a long time to keep the process alive and the file accessible.
        Thread.sleep(40000000);
    }

}
