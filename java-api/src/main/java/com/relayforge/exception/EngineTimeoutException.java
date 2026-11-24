package com.relayforge.exception;

public class EngineTimeoutException extends RuntimeException {
    public EngineTimeoutException(String message) {
        super(message);
    }

    public EngineTimeoutException(String message, Throwable cause) {
        super(message, cause);
    }
}
