package com.relayforge.exception;

public class EngineConnectionException extends RuntimeException {
    public EngineConnectionException(String message) {
        super(message);
    }

    public EngineConnectionException(String message, Throwable cause) {
        super(message, cause);
    }
}
