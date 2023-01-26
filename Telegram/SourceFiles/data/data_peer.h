/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_types.h"
#include "data/data_flags.h"
#include "data/notify/data_peer_notify_settings.h"
#include "data/data_cloud_file.h"
#include "ui/userpic_view.h"

struct BotInfo;
class PeerData;
class UserData;
class ChatData;
class ChannelData;

enum class ChatRestriction;

namespace Ui {
class EmptyUserpic;
} // namespace Ui

namespace Main {
class Account;
class Session;
} // namespace Main

namespace Data {

class Forum;
class ForumTopic;
class Session;
class GroupCall;
struct ReactionId;

[[nodiscard]] int PeerColorIndex(PeerId peerId);

// Must be used only for PeerColor-s.
[[nodiscard]] PeerId FakePeerIdForJustName(const QString &name);

class RestrictionCheckResult {
public:
	[[nodiscard]] static RestrictionCheckResult Allowed() {
		return { 0 };
	}
	[[nodiscard]] static RestrictionCheckResult WithEveryone() {
		return { 1 };
	}
	[[nodiscard]] static RestrictionCheckResult Explicit() {
		return { 2 };
	}

	explicit operator bool() const {
		return (_value != 0);
	}

	bool operator==(const RestrictionCheckResult &other) const {
		return (_value == other._value);
	}
	bool operator!=(const RestrictionCheckResult &other) const {
		return !(*this == other);
	}

	[[nodiscard]] bool isAllowed() const {
		return (*this == Allowed());
	}
	[[nodiscard]] bool isWithEveryone() const {
		return (*this == WithEveryone());
	}
	[[nodiscard]] bool isExplicit() const {
		return (*this == Explicit());
	}

private:
	RestrictionCheckResult(int value) : _value(value) {
	}

	int _value = 0;

};

struct UnavailableReason {
	QString reason;
	QString text;

	bool operator==(const UnavailableReason &other) const {
		return (reason == other.reason) && (text == other.text);
	}
	bool operator!=(const UnavailableReason &other) const {
		return !(*this == other);
	}
};

bool ApplyBotMenuButton(
	not_null<BotInfo*> info,
	const MTPBotMenuButton *button);

enum class AllowedReactionsType {
	All,
	Default,
	Some,
};

struct AllowedReactions {
	std::vector<ReactionId> some;
	AllowedReactionsType type = AllowedReactionsType::Some;
};

bool operator<(const AllowedReactions &a, const AllowedReactions &b);
bool operator==(const AllowedReactions &a, const AllowedReactions &b);

[[nodiscard]] AllowedReactions Parse(const MTPChatReactions &value);

} // namespace Data

class PeerClickHandler : public ClickHandler {
public:
	PeerClickHandler(not_null<PeerData*> peer);
	void onClick(ClickContext context) const override;

	not_null<PeerData*> peer() const {
		return _peer;
	}

private:
	not_null<PeerData*> _peer;

};

enum class PeerSetting {
	ReportSpam = (1 << 0),
	AddContact = (1 << 1),
	BlockContact = (1 << 2),
	ShareContact = (1 << 3),
	NeedContactsException = (1 << 4),
	AutoArchived = (1 << 5),
	RequestChat = (1 << 6),
	RequestChatIsBroadcast = (1 << 7),
	Unknown = (1 << 8),
};
inline constexpr bool is_flag_type(PeerSetting) { return true; };
using PeerSettings = base::flags<PeerSetting>;

class PeerData {
protected:
	PeerData(not_null<Data::Session*> owner, PeerId id);
	PeerData(const PeerData &other) = delete;
	PeerData &operator=(const PeerData &other) = delete;

public:
	using Settings = Data::Flags<PeerSettings>;

	virtual ~PeerData();

	static constexpr auto kServiceNotificationsId = peerFromUser(777000);

	[[nodiscard]] Data::Session &owner() const;
	[[nodiscard]] Main::Session &session() const;
	[[nodiscard]] Main::Account &account() const;

	[[nodiscard]] bool isUser() const {
		return peerIsUser(id);
	}
	[[nodiscard]] bool isChat() const {
		return peerIsChat(id);
	}
	[[nodiscard]] bool isChannel() const {
		return peerIsChannel(id);
	}
	[[nodiscard]] bool isSelf() const;
	[[nodiscard]] bool isVerified() const;
	[[nodiscard]] bool isPremium() const;
	[[nodiscard]] bool isScam() const;
	[[nodiscard]] bool isFake() const;
	[[nodiscard]] bool isMegagroup() const;
	[[nodiscard]] bool isBroadcast() const;
	[[nodiscard]] bool isForum() const;
	[[nodiscard]] bool isGigagroup() const;
	[[nodiscard]] bool isRepliesChat() const;
	[[nodiscard]] bool sharedMediaInfo() const {
		return isSelf() || isRepliesChat();
	}

	[[nodiscard]] bool isNotificationsUser() const {
		return (id == peerFromUser(333000))
			|| (id == kServiceNotificationsId);
	}
	[[nodiscard]] bool isServiceUser() const {
		return isUser() && !(id.value % 1000);
	}

	[[nodiscard]] Data::Forum *forum() const;
	[[nodiscard]] Data::ForumTopic *forumTopicFor(MsgId rootId) const;

	[[nodiscard]] Data::PeerNotifySettings &notify() {
		return _notify;
	}
	[[nodiscard]] const Data::PeerNotifySettings &notify() const {
		return _notify;
	}

	[[nodiscard]] bool allowsForwarding() const;
	[[nodiscard]] Data::RestrictionCheckResult amRestricted(
		ChatRestriction right) const;
	[[nodiscard]] bool amAnonymous() const;
	[[nodiscard]] bool canRevokeFullHistory() const;
	[[nodiscard]] bool slowmodeApplied() const;
	[[nodiscard]] rpl::producer<bool> slowmodeAppliedValue() const;
	[[nodiscard]] int slowmodeSecondsLeft() const;
	[[nodiscard]] bool canManageGroupCall() const;

	[[nodiscard]] UserData *asUser();
	[[nodiscard]] const UserData *asUser() const;
	[[nodiscard]] ChatData *asChat();
	[[nodiscard]] const ChatData *asChat() const;
	[[nodiscard]] ChannelData *asChannel();
	[[nodiscard]] const ChannelData *asChannel() const;
	[[nodiscard]] ChannelData *asMegagroup();
	[[nodiscard]] const ChannelData *asMegagroup() const;
	[[nodiscard]] ChannelData *asBroadcast();
	[[nodiscard]] const ChannelData *asBroadcast() const;
	[[nodiscard]] ChatData *asChatNotMigrated();
	[[nodiscard]] const ChatData *asChatNotMigrated() const;
	[[nodiscard]] ChannelData *asChannelOrMigrated();
	[[nodiscard]] const ChannelData *asChannelOrMigrated() const;

	[[nodiscard]] ChatData *migrateFrom() const;
	[[nodiscard]] ChannelData *migrateTo() const;
	[[nodiscard]] not_null<PeerData*> migrateToOrMe();
	[[nodiscard]] not_null<const PeerData*> migrateToOrMe() const;

	void updateFull();
	void updateFullForced();
	void fullUpdated();
	[[nodiscard]] bool wasFullUpdated() const {
		return (_lastFullUpdate != 0);
	}

	[[nodiscard]] int nameVersion() const;
	[[nodiscard]] const QString &name() const;
	[[nodiscard]] const QString &shortName() const;
	[[nodiscard]] const QString &topBarNameText() const;
	[[nodiscard]] QString userName() const;

	[[nodiscard]] const base::flat_set<QString> &nameWords() const {
		return _nameWords;
	}
	[[nodiscard]] const base::flat_set<QChar> &nameFirstLetters() const {
		return _nameFirstLetters;
	}

	void setUserpic(
		PhotoId photoId,
		const ImageLocation &location,
		bool hasVideo);
	void setUserpicPhoto(const MTPPhoto &data);
	void paintUserpic(
		Painter &p,
		Ui::PeerUserpicView &view,
		int x,
		int y,
		int size) const;
	void paintUserpicLeft(
			Painter &p,
			Ui::PeerUserpicView &view,
			int x,
			int y,
			int w,
			int size) const {
		paintUserpic(p, view, rtl() ? (w - x - size) : x, y, size);
	}
	void loadUserpic();
	[[nodiscard]] bool hasUserpic() const;
	[[nodiscard]] Ui::PeerUserpicView activeUserpicView();
	[[nodiscard]] Ui::PeerUserpicView createUserpicView();
	[[nodiscard]] bool useEmptyUserpic(Ui::PeerUserpicView &view) const;
	[[nodiscard]] InMemoryKey userpicUniqueKey(Ui::PeerUserpicView &view) const;
	[[nodiscard]] QImage generateUserpicImage(
		Ui::PeerUserpicView &view,
		int size,
		std::optional<int> radius = {}) const;
	[[nodiscard]] ImageLocation userpicLocation() const {
		return _userpic.location();
	}

	static constexpr auto kUnknownPhotoId = PhotoId(0xFFFFFFFFFFFFFFFFULL);
	[[nodiscard]] bool userpicPhotoUnknown() const {
		return (_userpicPhotoId == kUnknownPhotoId);
	}
	[[nodiscard]] PhotoId userpicPhotoId() const {
		return userpicPhotoUnknown() ? 0 : _userpicPhotoId;
	}
	[[nodiscard]] bool userpicHasVideo() const {
		return _userpicHasVideo;
	}
	[[nodiscard]] Data::FileOrigin userpicOrigin() const;
	[[nodiscard]] Data::FileOrigin userpicPhotoOrigin() const;

	// If this string is not empty we must not allow to open the
	// conversation and we must show this string instead.
	[[nodiscard]] QString computeUnavailableReason() const;

	[[nodiscard]] ClickHandlerPtr createOpenLink();
	[[nodiscard]] const ClickHandlerPtr &openLink() {
		if (!_openLink) {
			_openLink = createOpenLink();
		}
		return _openLink;
	}

	[[nodiscard]] QImage *userpicCloudImage(Ui::PeerUserpicView &view) const;

	[[nodiscard]] bool canPinMessages() const;
	[[nodiscard]] bool canEditMessagesIndefinitely() const;
	[[nodiscard]] bool canCreateTopics() const;
	[[nodiscard]] bool canManageTopics() const;
	[[nodiscard]] bool canExportChatHistory() const;

	// Returns true if about text was changed.
	bool setAbout(const QString &newAbout);
	[[nodiscard]] const QString &about() const {
		return _about;
	}

	void checkFolder(FolderId folderId);

	void setSettings(PeerSettings which) {
		_settings.set(which);
	}
	[[nodiscard]] auto settings() const {
		return (_settings.current() & PeerSetting::Unknown)
			? std::nullopt
			: std::make_optional(_settings.current());
	}
	[[nodiscard]] auto settingsValue() const {
		return (_settings.current() & PeerSetting::Unknown)
			? _settings.changes()
			: (_settings.value() | rpl::type_erased());
	}
	[[nodiscard]] QString requestChatTitle() const {
		return _requestChatTitle;
	}
	[[nodiscard]] TimeId requestChatDate() const {
		return _requestChatDate;
	}

	enum class TranslationFlag : uchar {
		Unknown,
		Disabled,
		Enabled,
	};
	void setTranslationDisabled(bool disabled);
	[[nodiscard]] TranslationFlag translationFlag() const;
	void saveTranslationDisabled(bool disabled);

	void setSettings(const MTPPeerSettings &data);

	enum class BlockStatus : char {
		Unknown,
		Blocked,
		NotBlocked,
	};
	[[nodiscard]] BlockStatus blockStatus() const {
		return _blockStatus;
	}
	[[nodiscard]] bool isBlocked() const {
		return (blockStatus() == BlockStatus::Blocked);
	}
	void setIsBlocked(bool is);

	enum class LoadedStatus : char {
		Not,
		Minimal,
		Normal,
		Full,
	};
	[[nodiscard]] LoadedStatus loadedStatus() const {
		return _loadedStatus;
	}
	[[nodiscard]] bool isMinimalLoaded() const {
		return (loadedStatus() != LoadedStatus::Not);
	}
	[[nodiscard]] bool isLoaded() const {
		return (loadedStatus() == LoadedStatus::Normal) || isFullLoaded();
	}
	[[nodiscard]] bool isFullLoaded() const {
		return (loadedStatus() == LoadedStatus::Full);
	}
	void setLoadedStatus(LoadedStatus status);

	[[nodiscard]] TimeId messagesTTL() const;
	void setMessagesTTL(TimeId period);

	[[nodiscard]] Data::GroupCall *groupCall() const;
	[[nodiscard]] PeerId groupCallDefaultJoinAs() const;

	void setThemeEmoji(const QString &emoticon);
	[[nodiscard]] const QString &themeEmoji() const;

	const PeerId id;
	MTPinputPeer input = MTP_inputPeerEmpty();

protected:
	void updateNameDelayed(
		const QString &newName,
		const QString &newNameOrPhone,
		const QString &newUsername);
	void updateUserpic(PhotoId photoId, MTP::DcId dcId, bool hasVideo);
	void clearUserpic();
	void invalidateEmptyUserpic();

private:
	void fillNames();
	[[nodiscard]] not_null<Ui::EmptyUserpic*> ensureEmptyUserpic() const;
	[[nodiscard]] virtual auto unavailableReasons() const
		-> const std::vector<Data::UnavailableReason> &;

	void setUserpicChecked(
		PhotoId photoId,
		const ImageLocation &location,
		bool hasVideo);

	const not_null<Data::Session*> _owner;

	mutable Data::CloudImage _userpic;
	PhotoId _userpicPhotoId = kUnknownPhotoId;

	mutable std::unique_ptr<Ui::EmptyUserpic> _userpicEmpty;

	Data::PeerNotifySettings _notify;

	ClickHandlerPtr _openLink;
	base::flat_set<QString> _nameWords; // for filtering
	base::flat_set<QChar> _nameFirstLetters;

	crl::time _lastFullUpdate = 0;

	QString _name;
	int _nameVersion = 1;

	TimeId _ttlPeriod = 0;

	Settings _settings = PeerSettings(PeerSetting::Unknown);
	BlockStatus _blockStatus = BlockStatus::Unknown;
	LoadedStatus _loadedStatus = LoadedStatus::Not;
	TranslationFlag _translationFlag = TranslationFlag::Unknown;
	bool _userpicHasVideo = false;

	QString _requestChatTitle;
	TimeId _requestChatDate = 0;

	QString _about;
	QString _themeEmoticon;

};

namespace Data {

void SetTopPinnedMessageId(
	not_null<PeerData*> peer,
	MsgId messageId);
[[nodiscard]] FullMsgId ResolveTopPinnedId(
	not_null<PeerData*> peer,
	MsgId topicRootId,
	PeerData *migrated = nullptr);
[[nodiscard]] FullMsgId ResolveMinPinnedId(
	not_null<PeerData*> peer,
	MsgId topicRootId,
	PeerData *migrated = nullptr);

} // namespace Data
