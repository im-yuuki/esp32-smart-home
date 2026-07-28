package com.smarthome.server.common;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import org.junit.jupiter.api.Test;

class UnicodeNamesTest {
    @Test
    void normalizesTrimmedVietnameseToNfc() {
        assertThat(UnicodeNames.normalize("  Chu\u031Ba pha\u0301t triê\u0309n  ", "name"))
                .isEqualTo("Chưa phát triển");
    }

    @Test
    void rejectsControlCharactersAndMoreThanOneHundredCodePoints() {
        assertThatThrownBy(() -> UnicodeNames.normalize("Phòng\nkhách", "name"))
                .isInstanceOf(IllegalArgumentException.class);
        assertThatThrownBy(() -> UnicodeNames.normalize("a".repeat(101), "name"))
                .isInstanceOf(IllegalArgumentException.class);
    }
}
