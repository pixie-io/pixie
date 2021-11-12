enablePlugins(PackPlugin)

lazy val root = (project in file(".")).
  settings(
    inThisBuild(List(
      scalaVersion := "2.12.12",
      version      := "1.0"
    )),
    name := "mux_test",
    libraryDependencies ++= Seq(
      "com.twitter" %% "finagle-http" % "21.4.0",
      "com.twitter" %% "scrooge-core" % "21.4.0",
      "org.apache.thrift" % "libthrift" % "0.10.0",
      "com.twitter" %% "finagle-thriftmux" % "21.4.0",
    ),
    Compile / packageBin / mainClass := Some("Server"),
  )
