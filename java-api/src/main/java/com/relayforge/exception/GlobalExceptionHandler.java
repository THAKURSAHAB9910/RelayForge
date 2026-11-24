package com.relayforge.exception;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

import java.net.URI;
import java.time.Instant;
import java.util.HashMap;
import java.util.Map;

@RestControllerAdvice
public class GlobalExceptionHandler {

    @ExceptionHandler(MethodArgumentNotValidException.class)
    public ProblemDetail handleValidationExceptions(MethodArgumentNotValidException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.BAD_REQUEST,
            "Request validation failed"
        );
        problem.setTitle("Validation Error");
        problem.setType(URI.create("https://relayforge.io/errors/validation-failed"));
        problem.setProperty("timestamp", Instant.now());

        Map<String, String> errors = new HashMap<>();
        for (FieldError fieldError : ex.getBindingResult().getFieldErrors()) {
            errors.put(fieldError.getField(), fieldError.getDefaultMessage());
        }
        problem.setProperty("invalid_fields", errors);
        return problem;
    }

    @ExceptionHandler(EngineConnectionException.class)
    public ProblemDetail handleEngineConnectionException(EngineConnectionException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.SERVICE_UNAVAILABLE,
            ex.getMessage()
        );
        problem.setTitle("C++ Engine Core Unavailable");
        problem.setType(URI.create("https://relayforge.io/errors/engine-unavailable"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("hint", "Ensure the C++ engine (relayforge_server) is running on the configured host/port.");
        return problem;
    }

    @ExceptionHandler(EngineTimeoutException.class)
    public ProblemDetail handleEngineTimeoutException(EngineTimeoutException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.GATEWAY_TIMEOUT,
            ex.getMessage()
        );
        problem.setTitle("C++ Engine Communication Timeout");
        problem.setType(URI.create("https://relayforge.io/errors/engine-timeout"));
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }

    @ExceptionHandler(EngineOverloadedException.class)
    public ProblemDetail handleEngineOverloadedException(EngineOverloadedException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.TOO_MANY_REQUESTS,
            ex.getMessage()
        );
        problem.setTitle("Engine Capacity Exceeded");
        problem.setType(URI.create("https://relayforge.io/errors/engine-overloaded"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("retry_after_seconds", 2);
        return problem;
    }

    @ExceptionHandler(ResourceNotFoundException.class)
    public ProblemDetail handleNotFoundException(ResourceNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.NOT_FOUND,
            ex.getMessage()
        );
        problem.setTitle("Resource Not Found");
        problem.setType(URI.create("https://relayforge.io/errors/not-found"));
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }

    @ExceptionHandler(Exception.class)
    public ProblemDetail handleGeneralException(Exception ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
            HttpStatus.INTERNAL_SERVER_ERROR,
            ex.getMessage() != null ? ex.getMessage() : "An unexpected server error occurred"
        );
        problem.setTitle("Internal Server Error");
        problem.setType(URI.create("https://relayforge.io/errors/internal-error"));
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }
}
