#!/usr/bin/env bash
#
# One-time Gradle setup for the self-hosted macOS runner (64 GB of memory).
# Run it once, as the runner user, from any directory:
#
#   bash .github/runner/setup-gradle.sh
#
# Why: `:app:packageDebug` reads every native library of the debug APK into
# the Gradle daemon's heap and dies with `OutOfMemoryError: Java heap space`
# when that heap is too small. Project gradle.properties cannot guarantee the
# heap on this machine (GRADLE_USER_HOME overrides it), and cancelled jobs
# leave "busy" zombie daemons behind that pin gigabytes for days.
#
# What it does, all outside the repo so it applies to every branch and every
# build on this machine:
#   1. sets the daemon JVM args in $GRADLE_USER_HOME/gradle.properties, which
#      takes precedence over any project gradle.properties
#   2. installs an init script in $GRADLE_USER_HOME/init.d that runs inside
#      every daemon and fails the build fast if the heap is not what we asked
#   3. installs a cron job that kills Gradle daemons older than three hours
#      (a build here takes well under an hour), and runs it once right away
#
# Re-running the script is safe: it replaces what it installed before.
set -euo pipefail

GRADLE_USER_HOME="${GRADLE_USER_HOME:-$HOME/.gradle}"
PROPERTIES="$GRADLE_USER_HOME/gradle.properties"
INIT_SCRIPT="$GRADLE_USER_HOME/init.d/check-daemon-heap.gradle"
KILLER="$HOME/bin/kill-stale-gradle-daemons"
JVMARGS="-Xmx16g -XX:MaxMetaspaceSize=1g -XX:+HeapDumpOnOutOfMemoryError -Dfile.encoding=UTF-8"

mkdir -p "$GRADLE_USER_HOME/init.d" "$HOME/bin"

# 1. Daemon heap for every build on this machine.
touch "$PROPERTIES"
grep -v '^org\.gradle\.jvmargs=' "$PROPERTIES" > "$PROPERTIES.tmp" || true
echo "org.gradle.jvmargs=$JVMARGS" >> "$PROPERTIES.tmp"
mv "$PROPERTIES.tmp" "$PROPERTIES"
echo "wrote org.gradle.jvmargs to $PROPERTIES"

# 2. Heap guard, executed inside the daemon at the start of every build.
cat > "$INIT_SCRIPT" <<'GRADLE'
// Installed by .github/runner/setup-gradle.sh (react-native-webgpu).
// Init scripts run inside the Gradle daemon, so this measures the heap the
// build actually gets and fails fast instead of as a flaky
// `java.lang.OutOfMemoryError: Java heap space` in `:app:packageDebug`.
if (gradle.parent == null) {
  def maxMb = (Runtime.runtime.maxMemory() / (1024L * 1024L)) as long
  println "Gradle daemon max heap: ${maxMb} MB"
  if (maxMb < 12 * 1024) {
    throw new GradleException("Gradle daemon max heap is ${maxMb} MB, expected at least 12 GB. Check org.gradle.jvmargs in ${gradle.gradleUserHomeDir}/gradle.properties.")
  }
}
GRADLE
echo "wrote $INIT_SCRIPT"

# 3. Zombie daemon killer, hourly via cron and once now.
cat > "$KILLER" <<'SH'
#!/usr/bin/env bash
# Installed by .github/runner/setup-gradle.sh (react-native-webgpu).
# Kills Gradle daemons that have been alive for more than three hours: no
# build on this runner legitimately runs that long, so those are zombies left
# behind by cancelled jobs, stuck "busy" forever and pinning heap.
ps -axo pid=,etime=,command= | awk '
  /GradleDaemon/ {
    # etime is [[dd-]hh:]mm:ss
    n = split($2, p, /[-:]/)
    if (n == 4) secs = p[1] * 86400 + p[2] * 3600 + p[3] * 60 + p[4]
    else if (n == 3) secs = p[1] * 3600 + p[2] * 60 + p[3]
    else secs = p[1] * 60 + p[2]
    if (secs > 3 * 3600) print $1
  }' | xargs -r kill -9 || true
SH
chmod +x "$KILLER"
(crontab -l 2>/dev/null | grep -v "kill-stale-gradle-daemons" || true; echo "0 * * * * $KILLER") | crontab -
echo "installed $KILLER (hourly via crontab)"
"$KILLER"

echo
echo "Gradle daemons still running:"
ps -axo pid=,etime=,command= | grep GradleDaemon | grep -v grep | cut -c1-100 || echo "  none"
echo
echo "Verify from a checkout: cd apps/example/android && ./gradlew help -q"
echo "It should print: Gradle daemon max heap: 16384 MB"
