# Stage 1: Build Java 21 Spring Boot JAR
FROM maven:3.9.6-eclipse-temurin-21 AS builder

WORKDIR /app
COPY java-api/pom.xml ./
RUN mvn dependency:go-offline -B

COPY java-api/src ./src
RUN mvn clean package -DskipTests -B

# Stage 2: Lightweight JRE Runtime
FROM eclipse-temurin:21-jre-jammy AS runtime

WORKDIR /opt/relayforge-api

COPY --from=builder /app/target/relayforge-java-api-*.jar app.jar

ENV RELAYFORGE_ENGINE_URL=http://cpp-engine:8080
ENV PORT=8000

EXPOSE 8000

HEALTHCHECK --interval=5s --timeout=3s --retries=3 \
    CMD curl -f http://127.0.0.1:8000/health || exit 1

ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:MaxRAMPercentage=75.0", "-jar", "app.jar"]
