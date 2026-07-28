package com.smarthome.server.authorization;

import java.io.Serializable;

public record FolderClosureId(Long ancestor, Long descendant) implements Serializable {}
