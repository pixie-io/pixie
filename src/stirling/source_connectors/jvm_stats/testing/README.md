# Java test files

HelloWorld.class is built from HelloWorld.java with an OpenJDK-compatible javac:
`javac src/stirling/source_connectors/jvm_stats/testing/HelloWorld.java`. You can run HelloWorld.class:
`java -cp src/stirling/source_connectors/jvm_stats/testing HelloWorld`.

Azul javac built .class cannot be loaded with OpenJDK JVM. But other versions of javac built .class
can still be loaded with Azul JVM. So this .class file was built with OpenJDK's javac.
