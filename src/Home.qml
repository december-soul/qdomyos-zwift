import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.12
import QtGraphicalEffects 1.12
import QtQuick.Window 2.12
import Qt.labs.settings 1.0

HomeForm{
    objectName: "home"
    signal start_clicked;
    signal stop_clicked;
    signal lap_clicked;
    signal plus_clicked(string name)
    signal minus_clicked(string name)

    Settings {
        id: settings
        property real ui_zoom: 100.0
    }

    Popup {
        id: popupLap
         parent: Overlay.overlay

         x: Math.round((parent.width - width) / 2)
         y: Math.round((parent.height - height) / 2)
         width: 380
         height: 60
         modal: true
         focus: true
         palette.text: "white"
         closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
         enter: Transition
         {
             NumberAnimation { property: "opacity"; from: 0.0; to: 1.0 }
         }
         exit: Transition
         {
             NumberAnimation { property: "opacity"; from: 1.0; to: 0.0 }
         }
         Column {
             anchors.horizontalCenter: parent.horizontalCenter
         Label {
             anchors.horizontalCenter: parent.horizontalCenter
             text: qsTr("New lap started!")
            }
         }
    }

    Timer {
        id: popupLapAutoClose
        interval: 2000; running: false; repeat: false
        onTriggered: popupLap.close();
    }

    Timer {
        id: refreshChartTimer
        interval: 1000
        running: true
        repeat: true
        property int timeline: 0
        onTriggered: {
            series1.upperSeries.append(timeline, rootItem.wattZ1);
            series2.upperSeries.append(timeline, rootItem.wattZ2);
            series3.upperSeries.append(timeline, rootItem.wattZ3);
            series4.upperSeries.append(timeline, rootItem.wattZ4);
            series5.upperSeries.append(timeline, rootItem.wattZ5);
            series6.upperSeries.append(timeline, rootItem.wattZ6);
            series7.upperSeries.append(timeline, rootItem.wattZ7);
            series1.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series2.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series3.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series4.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series5.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series6.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series7.axisX.min = (timeline - 60 > 0) ? timeline - 60 : 0;
            series1.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series2.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series3.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series4.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series5.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series6.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            series7.axisX.max = (timeline - 60 > 0) ? timeline : 60;
            /*series1.append(1, 5);
            series1.append(2, 50);
            series1.append(3, 500);*/
            //rootItem.dataSource.update(chartView.series(0));
            //console.log("refreshChartTimer" + timeline);
            timeline++;
        }
    }

    start.onClicked: { start_clicked(); }
    stop.onClicked: { stop_clicked(); }
    lap.onClicked: { lap_clicked(); popupLap.open(); popupLapAutoClose.running = true; }

    Component.onCompleted: { console.log("completed"); }

    GridView {
        anchors.horizontalCenter: parent.horizontalCenter
        cellWidth: 175 * settings.ui_zoom / 100
        cellHeight: 130 * settings.ui_zoom / 100
        anchors.fill: parent
        focus: true
        model: appModel
        leftMargin: { (parent.width % cellWidth) / 2 }
        anchors.topMargin: rootItem.topBarHeight + 30
        id: gridView
        objectName: "gridview"
        onMovementEnded: { headerToolbar.visible = (contentY == 0); }
        onWidthChanged: gridView.leftMargin = (parent.width % cellWidth) / 2;
        Screen.orientationUpdateMask:  Qt.LandscapeOrientation | Qt.PortraitOrientation
        Screen.onPrimaryOrientationChanged:{
                gridView.leftMargin = (parent.width % cellWidth) / 2;
        }

        //        highlight: Rectangle {
        //            width: 150
        //           height: 150
        //            color: "lightsteelblue"
        //        }
        delegate: Item {
            id: id1
            width: 170 * settings.ui_zoom / 100
            height: 125 * settings.ui_zoom / 100

            visible: visibleItem
            Component.onCompleted: console.log("completed " + objectName)

            Rectangle {
                width: 168 * settings.ui_zoom / 100
                height: 123 * settings.ui_zoom / 100
                radius: 3
                border.width: 1
                border.color: "purple"
                color: Material.backgroundColor
                id: rect
            }

            DropShadow {
                anchors.fill: rect
                cached: true
                horizontalOffset: 3
                verticalOffset: 3
                radius: 8.0
                samples: 16
                color: Material.color(Material.Purple)
                source: rect
            }

            Image {
                id: myIcon
                x: 5
                anchors {
                         bottom: id1.bottom
                }
                width: 48 * settings.ui_zoom / 100
                height: 48 * settings.ui_zoom / 100
                source: icon
            }
            Text {
                objectName: "value"
                id: myValue
                color: valueFontColor
                y: 0
                anchors {
                    horizontalCenter: parent.horizontalCenter
                }
                text: value
                horizontalAlignment: Text.AlignHCenter
                font.pointSize: valueFontSize * settings.ui_zoom / 100
                font.bold: true
            }
            Text {
                objectName: "secondLine"
                id: secondLineText
                color: "white"
                y: myValue.bottom
                anchors {
                    top: myValue.bottom
                    horizontalCenter: parent.horizontalCenter
                }
                text: secondLine
                horizontalAlignment: Text.AlignHCenter
                font.pointSize: 12 * settings.ui_zoom / 100
                font.bold: false
            }
            Text {
                id: myText
                anchors {
                    top: myIcon.top
                }
                font.bold: true
                     font.pointSize: labelFontSize
                color: "white"
                text: name
                anchors.left: parent.left
                anchors.leftMargin: 55 * settings.ui_zoom / 100
                anchors.topMargin: 20 * settings.ui_zoom / 100
            }
            RoundButton {
                objectName: minusName
                autoRepeat: true
                text: "-"
                onClicked: minus_clicked(objectName)
                visible: writable
                anchors.top: myValue.top
                anchors.left: parent.left
                anchors.leftMargin: 2
                width: 48 * settings.ui_zoom / 100
                height: 48 * settings.ui_zoom / 100
            }
            RoundButton {
                autoRepeat: true
                objectName: plusName
                text: "+"
                onClicked: plus_clicked(objectName)
                visible: writable
                anchors.top: myValue.top
                anchors.right: parent.right
                anchors.rightMargin: 2
                width: 48 * settings.ui_zoom / 100
                height: 48 * settings.ui_zoom / 100
            }

            /*MouseArea {
                anchors.fill: parent
                onClicked: parent.GridView.view.currentIndex = index
            }*/
        }
    }
}
