-- Help text for .summonfriend. Without a row in `command`,
-- ChatCommandNode::ResolveNames logs "Table `command` is missing help text" on
-- every startup. `security` must be 0 = SEC_PLAYER, the same level the module
-- registers the command with; otherwise the core overrides the table and logs it.
DELETE FROM `command` WHERE `name` = 'summonfriend';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('summonfriend', 0, 'Syntax: .summonfriend [$playername]\r\n\r\nSend a summon to a player in your group. They get the normal summon window and have two minutes to accept. Without a name, your current target is summoned.');
