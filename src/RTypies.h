#pragma once

namespace  RTypies {
	enum class WinDialog {
		Null,
		EditProperties,//属性管理
		AddMemberTimeRow,//给成员的个人时间表添加新的一行
		AddEditorTimeRow,//给成员的个人时间表添加新的一行
		PropertiesInfo,//打开【管理属性】页面
		CreateNew,//创建新的值班表
		EditMemberTimeRow,//编辑时间行
		EditEditorTimeRow,//编辑时间行
		BatchAddMembers,//批量添加成员
	};

	enum class Dialog {
		AddProperties,//添加新的属性
		
	};

	enum class Model {
		GetProperty,//获得属性列表
		AddNewProperty,//新加入属性
		RemoveProperty,//删除属性
		GetMemberAllData,//获得一个成员的memberdata（即所有信息）
		GetShowProperties,//获得需要展示出来的属性列表
		GetNote,//获得成员的备注内容

		GetScheduleModelLastDate,
	};

	enum class Page {
		UpdateShowPropertyList,//刷新可展示的属性的列表，即创建新的属性后把memberpage里面的属性列表盒更新
		UpdateInfoDockTimeRow,//更新成员信息浮窗的个人时间表的行
	};

	enum class Message {
		TimeIsNotValid,
		DuplicateAdd,
		Success,
		Failure,
	};

	enum class View {
		SingleClike,
		DoubleClick,
	};

}

