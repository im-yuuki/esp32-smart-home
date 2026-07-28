package com.smarthome.server.common;

import java.text.Normalizer;

public final class UnicodeNames {
    private static final int MAX_CODE_POINTS = 100;

    private UnicodeNames() {}

    public static String normalize(String value, String field) {
        if (value == null) {
            return null;
        }
        String normalized = Normalizer.normalize(value.trim(), Normalizer.Form.NFC);
        if (normalized.isEmpty()) {
            throw new IllegalArgumentException(field + " must not be blank");
        }
        if (normalized.codePointCount(0, normalized.length()) > MAX_CODE_POINTS) {
            throw new IllegalArgumentException(field + " must not exceed 100 code points");
        }
        if (normalized.codePoints().anyMatch(Character::isISOControl)) {
            throw new IllegalArgumentException(field + " must not contain control characters");
        }
        return normalized;
    }
}
