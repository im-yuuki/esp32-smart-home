package com.smarthome.server.authorization;

import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;

public interface FolderClosureRepository extends JpaRepository<FolderClosure, FolderClosureId> {
    @Query("select c.descendant.id from FolderClosure c where c.ancestor.id = :folderId order by c.depth, c.descendant.id")
    List<Long> findDescendantIds(Long folderId);

    @Query("select c.ancestor.id from FolderClosure c where c.descendant.id = :folderId order by c.depth")
    List<Long> findAncestorIds(Long folderId);

    boolean existsByAncestorIdAndDescendantId(Long ancestorId, Long descendantId);

    @Modifying
    @Query(value = "insert into folder_closure (ancestor_id, descendant_id, depth) values (:folderId, :folderId, 0)", nativeQuery = true)
    void insertSelf(Long folderId);

    @Modifying
    @Query(value = "delete path from folder_closure path join folder_closure subtree on subtree.ancestor_id = :folderId and subtree.descendant_id = path.descendant_id join folder_closure ancestors on ancestors.descendant_id = :folderId and ancestors.ancestor_id = path.ancestor_id where ancestors.ancestor_id <> ancestors.descendant_id", nativeQuery = true)
    void detachSubtree(Long folderId);

    @Modifying
    @Query(value = "insert into folder_closure (ancestor_id, descendant_id, depth) select super.ancestor_id, sub.descendant_id, super.depth + sub.depth + 1 from folder_closure super cross join folder_closure sub where super.descendant_id = :parentId and sub.ancestor_id = :folderId", nativeQuery = true)
    void attachSubtree(Long folderId, Long parentId);
}
