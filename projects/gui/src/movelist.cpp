/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "movelist.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <chessgame.h>

MoveList::MoveList(QWidget* parent)
	: QWidget(parent),
	  m_game(0),
	  m_moveCount(0),
	  m_startingSide(0),
	  m_selectedMove(-1),
	  m_moveToBeSelected(-1),
	  m_selectionTimer(new QTimer(this))
{
	m_moveList = new QTextBrowser(this);
	m_moveList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_moveList->setOpenLinks(false);
	m_moveList->document()->setDefaultStyleSheet(
		"a:link { text-decoration: none; } "
		".comment { color: grey; }");
	connect(m_moveList, SIGNAL(anchorClicked(const QUrl&)), this,
	    SLOT(onMoveOrCommentClicked(const QUrl&)));

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addWidget(m_moveList);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	m_selectionTimer->setSingleShot(true);
	m_selectionTimer->setInterval(50);
	connect(m_selectionTimer, SIGNAL(timeout()),
		this, SLOT(selectChosenMove()));
}

MoveList::HtmlMove MoveList::htmlMove(int moveNum,
				      int startingSide,
				      const QString& moveString,
				      const QString& comment)
{
	HtmlMove htmlMove;

	if (moveNum == 0 && startingSide == Chess::Side::Black)
		htmlMove.number = "<strong>1...</strong> ";
	else
	{
		moveNum += startingSide;
		if (moveNum % 2 == 0)
			htmlMove.number = QString("<strong>%1.</strong> ")
					  .arg(moveNum / 2 + 1);
	}

	htmlMove.move = QString("<a href=\"move://%1@\">%2</a> ")
			.arg(moveNum).arg(moveString);
	#ifndef Q_OS_WIN32
	htmlMove.move.replace('-', "&#8288;-&#8288;");
	#endif

	if (!comment.isEmpty())
		htmlMove.comment = QString("<a class=\"comment\" href=\"comment://%1@\">%2</a> ")
				   .arg(moveNum).arg(comment);

	return htmlMove;
}

void MoveList::setGame(ChessGame* game, PgnGame* pgn)
{
	if (m_game != 0)
		m_game->disconnect(this);
	m_game = game;

	if (pgn == 0)
	{
		Q_ASSERT(game != 0);
		pgn = m_game->pgn();
	}

	m_moveList->clear();
	m_movePos.clear();
	m_selectedMove = -1;
	m_moveToBeSelected = -1;

	m_startingSide = pgn->startingSide();
	for (m_moveCount = 0; m_moveCount < pgn->moves().size(); m_moveCount++)
	{
		const PgnGame::MoveData& md = pgn->moves().at(m_moveCount);
		insertHtmlMove(htmlMove(m_moveCount, m_startingSide,
					md.moveString, md.comment));
	}

	if (m_game != 0)
		connect(m_game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)),
			this, SLOT(onMoveMade(Chess::GenericMove, QString, QString)));

	QScrollBar* sb = m_moveList->verticalScrollBar();
	sb->setValue(sb->maximum());

	selectMove(m_moveCount - 1);
}

void MoveList::onMoveMade(const Chess::GenericMove& genericMove,
			  const QString& sanString,
			  const QString& comment)
{
	Q_UNUSED(genericMove);

	QScrollBar* sb = m_moveList->verticalScrollBar();
	bool atEnd = sb->value() == sb->maximum();

	insertHtmlMove(htmlMove(m_moveCount++, m_startingSide, sanString, comment));

	bool atLastMove = false;
	if (m_selectedMove == -1 || m_moveToBeSelected == m_moveCount - 2)
		atLastMove = true;
	if (m_moveToBeSelected == -1 && m_selectedMove == m_moveCount - 2)
		atLastMove = true;

	if (atLastMove)
		selectMove(m_moveCount - 1);

	if (atEnd && atLastMove)
		sb->setValue(sb->maximum());
}

void MoveList::insertHtmlMove(const HtmlMove& htmlMove)
{
	QTextCursor cursor = m_moveList->textCursor();
	cursor.movePosition(QTextCursor::End);

	cursor.insertHtml(htmlMove.number);

	QPair<int, int> movePos;
	movePos.first = cursor.position();
	cursor.insertHtml(htmlMove.move);
	movePos.second = cursor.position() - 1;
	if (movePos.second > 0)
		m_movePos.append(movePos);

	cursor.insertHtml(htmlMove.comment);
}

void MoveList::selectChosenMove()
{
	int moveNum = m_moveToBeSelected;
	m_moveToBeSelected = -1;
	Q_ASSERT(moveNum >= 0 && moveNum < m_moveCount);

	if (m_selectedMove >= 0)
	{
		QTextCursor c = m_moveList->textCursor();
		c.setPosition(m_movePos.at(m_selectedMove).first);
		c.setPosition(m_movePos.at(m_selectedMove).second, QTextCursor::KeepAnchor);
		c.mergeCharFormat(m_defaultTextFormat);
	}

	m_selectedMove = moveNum;
	QTextCursor c = m_moveList->textCursor();
	c.setPosition(m_movePos.at(moveNum).first);
	c.setPosition(m_movePos.at(moveNum).second, QTextCursor::KeepAnchor);

	QTextCharFormat format(c.charFormat());
	m_defaultTextFormat = format;
	m_defaultTextFormat.setBackground(format.background());
	m_defaultTextFormat.setForeground(format.foreground());

	format.setForeground(Qt::white);
	format.setBackground(Qt::black);
	c.mergeCharFormat(format);
}

void MoveList::selectMove(int moveNum)
{
	if (moveNum == -1)
		moveNum = 0;
	if (moveNum >= m_moveCount || m_moveCount <= 0)
		return;

	m_moveToBeSelected = moveNum;
	m_selectionTimer->start();
}

void MoveList::onMoveOrCommentClicked(const QUrl& url)
{
	bool ok;
	int moveNum = url.userName().toInt(&ok);

	if (!ok)
	{
		qWarning("MoveList: invalid move number: %s",
		    qPrintable(url.userName()));

		return;
	}

	if (url.scheme() == "move")
		emit moveClicked(moveNum);
	else if (url.scheme() == "comment")
	{
		emit commentClicked(moveNum);
		return;
	}
	else
		qWarning("MoveList: unknown scheme: %s",
		    qPrintable(url.scheme()));

	selectMove(moveNum);
}
