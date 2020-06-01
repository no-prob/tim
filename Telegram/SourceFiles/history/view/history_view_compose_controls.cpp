/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_compose_controls.h"

#include "ui/widgets/input_fields.h"
#include "ui/special_buttons.h"
#include "ui/text_options.h"
#include "ui/ui_utility.h"
#include "lang/lang_keys.h"
#include "base/event_filter.h"
#include "base/call_delayed.h"
#include "base/qt_signal_producer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "chat_helpers/tabbed_panel.h"
#include "chat_helpers/tabbed_section.h"
#include "chat_helpers/tabbed_selector.h"
#include "chat_helpers/message_field.h"
#include "chat_helpers/emoji_suggestions_widget.h"
#include "window/window_session_controller.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "inline_bots/inline_results_widget.h"
#include "facades.h"
#include "styles/style_history.h"

namespace HistoryView {

namespace {

using MessageToEdit = ComposeControls::MessageToEdit;

} // namespace

class FieldHeader : public Ui::RpWidget {
public:
	FieldHeader(QWidget *parent, not_null<Data::Session*> data);

	void editMessage(FullMsgId edit);

	bool isDisplayed() const;
	bool isEditingMessage() const;
	rpl::producer<FullMsgId> editMsgId() const;
	MessageToEdit queryToEdit();

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void updateControlsGeometry(QSize size);

	Ui::Text::String _editMsgText;
	rpl::variable<FullMsgId> _editMsgId;

	const not_null<Data::Session*> _data;
	const not_null<Ui::IconButton*> _cancel;

};

FieldHeader::FieldHeader(QWidget *parent, not_null<Data::Session*> data)
: RpWidget(parent)
, _data(data)
, _cancel(Ui::CreateChild<Ui::IconButton>(this, st::historyReplyCancel)) {
	resize(QSize(parent->width(), st::historyReplyHeight));

	sizeValue(
	) | rpl::start_with_next([=](QSize size) {
		updateControlsGeometry(size);
	}, lifetime());

	_editMsgId.value(
	) | rpl::start_with_next([=] {
		isDisplayed() ? show() : hide();

		if (const auto item = _data->message(_editMsgId.current())) {
			_editMsgText.setText(
				st::messageTextStyle,
				item->inReplyText(),
				Ui::DialogTextOptions());
		}
	}, lifetime());

	_cancel->addClickHandler([=] {
		_editMsgId = {};
	});
}

void FieldHeader::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto replySkip = st::historyReplySkip;

	p.fillRect(rect(), st::historyComposeAreaBg);

	st::historyEditIcon.paint(p, st::historyReplyIconPosition, width());

	p.setPen(st::historyReplyNameFg);
	p.setFont(st::msgServiceNameFont);
	p.drawTextLeft(
		replySkip,
		st::msgReplyPadding.top(),
		width(),
		tr::lng_edit_message(tr::now));

	p.setPen(st::historyComposeAreaFg);
	p.setTextPalette(st::historyComposeAreaPalette);
	_editMsgText.drawElided(
		p,
		replySkip,
		st::msgReplyPadding.top() + st::msgServiceNameFont->height,
		width() - replySkip - _cancel->width() - st::msgReplyPadding.right());
	p.restoreTextPalette();
}

bool FieldHeader::isDisplayed() const {
	return isEditingMessage();
}

bool FieldHeader::isEditingMessage() const {
	return !!_editMsgId.current();
}

void FieldHeader::updateControlsGeometry(QSize size) {
	_cancel->moveToRight(0, 0);
}

void FieldHeader::editMessage(FullMsgId id) {
	_editMsgId = id;
}

rpl::producer<FullMsgId> FieldHeader::editMsgId() const {
	return _editMsgId.value();
}

MessageToEdit FieldHeader::queryToEdit() {
	const auto item = _data->message(_editMsgId.current());
	if (!isEditingMessage() || !item) {
		return {};
	}
	return {
		item->fullId(),
		{
			item->isScheduled() ? item->date() : 0,
			false,
			false,
			false,
		},
	};
}

ComposeControls::ComposeControls(
	not_null<QWidget*> parent,
	not_null<Window::SessionController*> window,
	Mode mode)
: _parent(parent)
, _window(window)
//, _mode(mode)
, _wrap(std::make_unique<Ui::RpWidget>(parent))
, _send(Ui::CreateChild<Ui::SendButton>(_wrap.get()))
, _attachToggle(Ui::CreateChild<Ui::IconButton>(
	_wrap.get(),
	st::historyAttach))
, _tabbedSelectorToggle(Ui::CreateChild<Ui::EmojiButton>(
	_wrap.get(),
	st::historyAttachEmoji))
, _field(
	Ui::CreateChild<Ui::InputField>(
		_wrap.get(),
		st::historyComposeField,
		Ui::InputField::Mode::MultiLine,
		tr::lng_message_ph()))
, _header(std::make_unique<FieldHeader>(
		_wrap.get(),
		&_window->session().data())) {
	init();
}

ComposeControls::~ComposeControls() {
	setTabbedPanel(nullptr);
}

Main::Session &ComposeControls::session() const {
	return _window->session();
}

void ComposeControls::setHistory(History *history) {
	if (_history == history) {
		return;
	}
	_history = history;
	_window->tabbedSelector()->setCurrentPeer(
		history ? history->peer.get() : nullptr);
}

void ComposeControls::move(int x, int y) {
	_wrap->move(x, y);
}

void ComposeControls::resizeToWidth(int width) {
	_wrap->resizeToWidth(width);
	updateHeight();
}

rpl::producer<int> ComposeControls::height() const {
	return _wrap->heightValue();
}

int ComposeControls::heightCurrent() const {
	return _wrap->height();
}

void ComposeControls::focus() {
	_field->setFocus();
}

rpl::producer<> ComposeControls::cancelRequests() const {
	return _cancelRequests.events();
}

rpl::producer<> ComposeControls::sendRequests() const {
	auto filter = rpl::filter([=] {
		return _send->type() == Ui::SendButton::Type::Schedule;
	});
	auto submits = base::qt_signal_producer(
		_field.get(),
		&Ui::InputField::submitted);
	return rpl::merge(
		_send->clicks() | filter | rpl::to_empty,
		std::move(submits) | filter | rpl::to_empty);
}

rpl::producer<MessageToEdit> ComposeControls::editRequests() const {
	auto toValue = rpl::map([=] { return _header->queryToEdit(); });
	auto filter = rpl::filter([=] {
		return _send->type() == Ui::SendButton::Type::Save;
	});
	auto submits = base::qt_signal_producer(
		_field.get(),
		&Ui::InputField::submitted);
	return rpl::merge(
		_send->clicks() | filter | toValue,
		std::move(submits) | filter | toValue);
}

rpl::producer<> ComposeControls::attachRequests() const {
	return _attachToggle->clicks() | rpl::to_empty;
}

void ComposeControls::setMimeDataHook(MimeDataHook hook) {
	_field->setMimeDataHook(std::move(hook));
}

rpl::producer<not_null<DocumentData*>> ComposeControls::fileChosen() const {
	return _fileChosen.events();
}

rpl::producer<not_null<PhotoData*>> ComposeControls::photoChosen() const {
	return _photoChosen.events();
}

auto ComposeControls::inlineResultChosen() const
->rpl::producer<ChatHelpers::TabbedSelector::InlineChosen> {
	return _inlineResultChosen.events();
}

void ComposeControls::showStarted() {
	if (_inlineResults) {
		_inlineResults->hideFast();
	}
	if (_tabbedPanel) {
		_tabbedPanel->hideFast();
	}
	_wrap->hide();
}

void ComposeControls::showFinished() {
	if (_inlineResults) {
		_inlineResults->hideFast();
	}
	if (_tabbedPanel) {
		_tabbedPanel->hideFast();
	}
	_wrap->show();
}

void ComposeControls::showForGrab() {
	showFinished();
}

TextWithTags ComposeControls::getTextWithAppliedMarkdown() const {
	return _field->getTextWithAppliedMarkdown();
}

void ComposeControls::clear() {
	setText(TextWithTags());
}

void ComposeControls::setText(const TextWithTags &textWithTags) {
	//_textUpdateEvents = events;
	_field->setTextWithTags(textWithTags, Ui::InputField::HistoryAction::Clear/*fieldHistoryAction*/);
	auto cursor = _field->textCursor();
	cursor.movePosition(QTextCursor::End);
	_field->setTextCursor(cursor);
	//_textUpdateEvents = TextUpdateEvent::SaveDraft
	//	| TextUpdateEvent::SendTyping;

	//previewCancel();
	//_previewCancelled = false;
}

void ComposeControls::hidePanelsAnimated() {
	//_fieldAutocomplete->hideAnimated();
	if (_tabbedPanel) {
		_tabbedPanel->hideAnimated();
	}
	if (_inlineResults) {
		_inlineResults->hideAnimated();
	}
}

void ComposeControls::init() {
	initField();
	initTabbedSelector();
	initSendButton();

	_wrap->sizeValue(
	) | rpl::start_with_next([=](QSize size) {
		updateControlsGeometry(size);
	}, _wrap->lifetime());

	_wrap->geometryValue(
	) | rpl::start_with_next([=](QRect rect) {
		updateOuterGeometry(rect);
	}, _wrap->lifetime());

	_wrap->paintRequest(
	) | rpl::start_with_next([=](QRect clip) {
		paintBackground(clip);
	}, _wrap->lifetime());

	_header->editMsgId(
	) | rpl::start_with_next([=](auto id) {
		updateHeight();
		updateSendButtonType();

		if (_header->isEditingMessage()) {
			setTextFromEditingMessage(_window->session().data().message(id));
		} else {
			setText(_localSavedText);
			_localSavedText = {};
		}
	}, _wrap->lifetime());
}

void ComposeControls::setTextFromEditingMessage(not_null<HistoryItem*> item) {
	if (!_header->isEditingMessage()) {
		return;
	}
	_localSavedText = getTextWithAppliedMarkdown();
	const auto t = item->originalText();
	const auto text = TextWithTags{
		t.text,
		TextUtilities::ConvertEntitiesToTextTags(t.entities)
	};
	setText(text);
}

void ComposeControls::initField() {
	_field->setMaxHeight(st::historyComposeFieldMaxHeight);
	_field->setSubmitSettings(Core::App().settings().sendSubmitWay());
	//Ui::Connect(_field, &Ui::InputField::submitted, [=] { send(); });
	Ui::Connect(_field, &Ui::InputField::cancelled, [=] { escape(); });
	//Ui::Connect(_field, &Ui::InputField::tabbed, [=] { fieldTabbed(); });
	Ui::Connect(_field, &Ui::InputField::resized, [=] { updateHeight(); });
	//Ui::Connect(_field, &Ui::InputField::focused, [=] { fieldFocused(); });
	//Ui::Connect(_field, &Ui::InputField::changed, [=] { fieldChanged(); });
	InitMessageField(_window, _field);
	const auto suggestions = Ui::Emoji::SuggestionsController::Init(
		_parent,
		_field,
		&_window->session());
	_raiseEmojiSuggestions = [=] { suggestions->raise(); };
}

void ComposeControls::initTabbedSelector() {
	if (_window->hasTabbedSelectorOwnership()) {
		createTabbedPanel();
	} else {
		setTabbedPanel(nullptr);
	}

	_tabbedSelectorToggle->addClickHandler([=] {
		toggleTabbedSelectorMode();
	});

	const auto selector = _window->tabbedSelector();
	const auto wrap = _wrap.get();

	base::install_event_filter(wrap, selector, [=](not_null<QEvent*> e) {
		if (_tabbedPanel && e->type() == QEvent::ParentChange) {
			setTabbedPanel(nullptr);
		}
		return base::EventFilterResult::Continue;
	});

	selector->emojiChosen(
	) | rpl::start_with_next([=](EmojiPtr emoji) {
		Ui::InsertEmojiAtCursor(_field->textCursor(), emoji);
	}, wrap->lifetime());

	selector->fileChosen(
	) | rpl::start_to_stream(_fileChosen, wrap->lifetime());

	selector->photoChosen(
	) | rpl::start_to_stream(_photoChosen, wrap->lifetime());

	selector->inlineResultChosen(
	) | rpl::start_to_stream(_inlineResultChosen, wrap->lifetime());
}

void ComposeControls::initSendButton() {
	updateSendButtonType();
	_send->finishAnimating();
}

void ComposeControls::updateSendButtonType() {
	using Type = Ui::SendButton::Type;
	const auto type = [&] {
		if (_header->isEditingMessage()) {
			return Type::Save;
		//} else if (_isInlineBot) {
		//	return Type::Cancel;
		//} else if (showRecordButton()) {
		//	return Type::Record;
		}
		return Type::Schedule;
	}();
	_send->setType(type);

	const auto delay = [&] {
		return /*(type != Type::Cancel && type != Type::Save && _peer)
			? _peer->slowmodeSecondsLeft()
			: */0;
	}();
	_send->setSlowmodeDelay(delay);
	//_send->setDisabled(_peer
	//	&& _peer->slowmodeApplied()
	//	&& (_history->latestSendingMessage() != nullptr)
	//	&& (type == Type::Send || type == Type::Record));

	//if (delay != 0) {
	//	base::call_delayed(
	//		kRefreshSlowmodeLabelTimeout,
	//		this,
	//		[=] { updateSendButtonType(); });
	//}
}

void ComposeControls::updateControlsGeometry(QSize size) {
	// _attachToggle -- _inlineResults ------ _tabbedPanel -- _fieldBarCancel
	// (_attachDocument|_attachPhoto) _field _tabbedSelectorToggle _send

	const auto fieldWidth = size.width()
		- _attachToggle->width()
		- st::historySendRight
		- _send->width()
		- _tabbedSelectorToggle->width();
	_field->resizeToWidth(fieldWidth);

	const auto buttonsTop = size.height() - _attachToggle->height();

	auto left = 0;
	_attachToggle->moveToLeft(left, buttonsTop);
	left += _attachToggle->width();
	_field->moveToLeft(
		left,
		size.height() - _field->height() - st::historySendPadding);

	_header->resizeToWidth(size.width());
	_header->moveToLeft(
		0,
		_field->y() - _header->height() - st::historySendPadding);

	auto right = st::historySendRight;
	_send->moveToRight(right, buttonsTop);
	right += _send->width();
	_tabbedSelectorToggle->moveToRight(right, buttonsTop);
}

void ComposeControls::updateOuterGeometry(QRect rect) {
	if (_inlineResults) {
		_inlineResults->moveBottom(rect.y());
	}
	if (_tabbedPanel) {
		_tabbedPanel->moveBottomRight(
			rect.y() + rect.height() - _attachToggle->height(),
			rect.x() + rect.width());
	}
}

void ComposeControls::paintBackground(QRect clip) {
	Painter p(_wrap.get());

	p.fillRect(clip, st::historyComposeAreaBg);
}

void ComposeControls::escape() {
	_cancelRequests.fire({});
}

bool ComposeControls::pushTabbedSelectorToThirdSection(
		not_null<PeerData*> peer,
		const Window::SectionShow &params) {
	if (!_tabbedPanel) {
		return true;
	//} else if (!_canSendMessages) {
	//	Core::App().settings().setTabbedReplacedWithInfo(true);
	//	_window->showPeerInfo(_peer, params.withThirdColumn());
	//	return;
	}
	Core::App().settings().setTabbedReplacedWithInfo(false);
	_tabbedSelectorToggle->setColorOverrides(
		&st::historyAttachEmojiActive,
		&st::historyRecordVoiceFgActive,
		&st::historyRecordVoiceRippleBgActive);
	_window->resizeForThirdSection();
	_window->showSection(
		ChatHelpers::TabbedMemento(),
		params.withThirdColumn());
	return true;
}

bool ComposeControls::returnTabbedSelector() {
	createTabbedPanel();
	updateOuterGeometry(_wrap->geometry());
	return true;
}

void ComposeControls::createTabbedPanel() {
	setTabbedPanel(std::make_unique<ChatHelpers::TabbedPanel>(
		_parent,
		_window,
		_window->tabbedSelector()));
}

void ComposeControls::setTabbedPanel(
		std::unique_ptr<ChatHelpers::TabbedPanel> panel) {
	_tabbedPanel = std::move(panel);
	if (const auto raw = _tabbedPanel.get()) {
		_tabbedSelectorToggle->installEventFilter(raw);
		_tabbedSelectorToggle->setColorOverrides(nullptr, nullptr, nullptr);
	} else {
		_tabbedSelectorToggle->setColorOverrides(
			&st::historyAttachEmojiActive,
			&st::historyRecordVoiceFgActive,
			&st::historyRecordVoiceRippleBgActive);
	}
}

void ComposeControls::toggleTabbedSelectorMode() {
	if (!_history) {
		return;
	}
	if (_tabbedPanel) {
		if (_window->canShowThirdSection() && !Adaptive::OneColumn()) {
			Core::App().settings().setTabbedSelectorSectionEnabled(true);
			Core::App().saveSettingsDelayed();
			pushTabbedSelectorToThirdSection(
				_history->peer,
				Window::SectionShow::Way::ClearStack);
		} else {
			_tabbedPanel->toggleAnimated();
		}
	} else {
		_window->closeThirdSection();
	}
}

void ComposeControls::updateHeight() {
	const auto height = _field->height()
		+ (_header->isDisplayed() ? _header->height() : 0)
		+ 2 * st::historySendPadding;
	_wrap->resize(_wrap->width(), height);
}

void ComposeControls::editMessage(FullMsgId edit) {
	cancelEditMessage();
	_header->editMessage(std::move(edit));
}

void ComposeControls::cancelEditMessage() {
	_header->editMessage({});
}

} // namespace HistoryView
