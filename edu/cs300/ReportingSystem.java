package edu.cs300;

import java.io.File;
import java.io.FileWriter;
import java.io.FileNotFoundException;
import java.util.Enumeration;
import java.util.Scanner;
import java.util.Vector;
import java.util.StringTokenizer;
import java.util.concurrent.*;

class Column {
	int leftBound;
	int rightBound;
	String columnName;
}

class ReportGenerator extends Thread {

	private int threadID;
	int reportCount;	//	same as index as specified in the pdf
	String filePath;

	//	Information and report details provided by file given
	String title;
	String searchString;
	String outputFileName;
	Vector<Column> columnList = new Vector<Column>();
	Vector<String> records = new Vector<String>();



	public ReportGenerator(int threadID, int reportCount, String filePath) {
		this.threadID = threadID;
		this.reportCount = reportCount;
		this.filePath = filePath;
	}


	@Override
	public void run() {
		
		// int index = 0;
		try {
			File file = new File(filePath);
			Scanner report = new Scanner(file);

			title = report.nextLine();
			searchString = report.nextLine();
			outputFileName = report.nextLine();

			
			String line;

			while(!((line = report.nextLine()).isEmpty())) {

				Scanner lineScanner = new Scanner(line);
				lineScanner.useDelimiter("-|,|\n");


				int left = lineScanner.nextInt();
				int right = lineScanner.nextInt();

				String columnTitle = lineScanner.next();
				Column newColumn = new Column();

				newColumn.leftBound = left;
				newColumn.rightBound = right;
				newColumn.columnName = columnTitle;


				columnList.add(newColumn); // report parameter to vector

			}	

		} catch (FileNotFoundException ex) {
			System.out.println("FileNotFoundException triggered:"+ex.getMessage());
		}

		

		//	send report request and recieve records back
		try {

			//	send report request
			MessageJNI.writeReportRequest(threadID, reportCount, searchString);

			//	keeps string that holds the current record being read in
			String reportRecord;

			//	reads records and adds them to records vector until empty string is sent
			while(!((reportRecord = MessageJNI.readReportRecord(threadID)).isEmpty())) {
				records.add(reportRecord);				
			}

		} catch (Exception e) {
			System.err.println("Error: " + e);
		}
	
		
		// write to file	
		try {
			// FileWriter outputFile = new FileWriter(tempFileName);	//	update later to include
			FileWriter outputFile = new FileWriter(outputFileName);
			String recordString;

			//	write title to file
			outputFile.write(title + "\n");

			//	writing column names to file
			for(int i = 0; i < columnList.size(); i++) {
				// writeString = writeString.concat(columnList.get(i).columnName + "\t");
				outputFile.write(columnList.get(i).columnName + "\t");
			}
			// outputFile.write(writeString + "\n");
			outputFile.write("\n");


			//	for loop for iterate through records
			for(int i = 0; i < records.size(); i++) {

				recordString = records.get(i);

				//	for loop to print parse and write to file
				for(int j = 0; j < columnList.size(); j++) {
					outputFile.write(recordString.substring(columnList.get(j).leftBound - 1, columnList.get(j).rightBound) + "\t");
				}
				outputFile.write("\n");
			}
			outputFile.close();

		} catch(Exception e) {
			System.out.println("Error: " + e);
		} 


	}

	public static native void writeReportRequest(int reportIdx, int reportCount, String searchString);
	public static native String readReportRecord(int qid);
	// private static native String readStringMsg();


}

public class ReportingSystem {

	ReportingSystem() {
		DebugLog.log("Starting Reporting System");
	}

	public int loadReportJobs() {
		int reportCounter = 0;
		try {

				// File file = new File ("/home/jfrances/f21_os_project/report_list.txt");
				File file = new File("report_list.txt");

				Scanner reportList = new Scanner(file);

				int processCount = reportList.nextInt();
				reportList.nextLine();
				reportCounter = processCount;

				for(int i = 0; i < processCount; i++) {
					if(reportList.hasNextLine()) {
						String fileName = reportList.nextLine();
						ReportGenerator thread = new ReportGenerator(i + 1, reportCounter, fileName);
						thread.start();
					} else {
						System.out.println("Error in report_list.txt file");
					}
				}

		

 		     	//	load specs and create threads for each report
				//	starting at line 2 index = 1, line 3 index = 2 ...
				// thread parses specification file ie: report1.txt, sends record request and receives records
				DebugLog.log("Load specs and create threads for each report\nStart thread to request, process and print reports");

				reportList.close();
		} catch (FileNotFoundException ex) {
				System.out.println("FileNotFoundException triggered:"+ex.getMessage());
		}
		return reportCounter;

	}

	public static void main(String[] args) throws FileNotFoundException {


			ReportingSystem reportSystem= new ReportingSystem();
			int reportCount = reportSystem.loadReportJobs();



	}


	public static native void writeReportRequest(int reportIdx, int reportCount, String searchString);
	public static native String readReportRecord(int qid);


}
